// Copyright 2013 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://sourceforge.net/projects/jsoncons/files/ for latest version
// See https://sourceforge.net/p/jsoncons/wiki/Home/ for documentation.

#ifndef JSONCONS_JSON1_HPP
#define JSONCONS_JSON1_HPP

#include <limits>
#include <string>
#include <vector>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <memory>
#include <array>
#include <typeinfo>
#include "json_structures.hpp"
#include "jsoncons/jsoncons.hpp"
#include "jsoncons/json_output_handler.hpp"
#include "jsoncons/output_format.hpp"
#include "jsoncons/json_serializer.hpp"
#include "jsoncons/json_deserializer.hpp"
#include "jsoncons/json_reader.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif

namespace jsoncons {

template <typename Char,class T> inline
void serialize(basic_json_output_handler<Char>& os, const T&)
{
    os.value(null_type());
}

template <typename Char, class Alloc>
class basic_json;

template <typename Char>
std::basic_string<Char> escape_string(const std::basic_string<Char>& s, const basic_output_format<Char>& format);

template <typename Char>
class basic_parse_error_handler;

template <typename Char, typename Alloc, typename T>
class json_type_traits
{
public:
    static bool is(const basic_json<Char,Alloc>&)
    {
        return false;
    }
};

namespace value_types
{
    enum value_types_t 
    {
        empty_object_t,
        object_t,
        array_t,
        small_string_t,
        string_t,
        double_t,
        integer_t,
        uinteger_t,
        bool_t,
        null_t,
        any_t
    };
}

template <typename Char, typename Alloc = std::allocator<void>>
class basic_json
{
public:

    typedef Alloc allocator_type;

    typedef json_array<basic_json<Char,Alloc>,Alloc> array;
    typedef json_object<basic_json<Char,Alloc>,Alloc> object;

    typedef Char char_type;

    typedef jsoncons::null_type null_type;

    // Allocation
    static void* operator new(std::size_t) { return typename Alloc::template rebind<basic_json>::other().allocate(1); }
    static void operator delete(void* ptr) { return typename Alloc::template rebind<basic_json>::other().deallocate(static_cast<basic_json*>(ptr), 1); }
    static void* operator new( std::size_t s, void* p ) throw()
    {
        return ::operator new( s, p );
    }

    class any
    {
    public:
        any()
            : impl_(nullptr)
        {
        }
        any(const any& val)
        {
			impl_ = val.impl_ != nullptr ? val.impl_->clone() : nullptr;
        }
        any(any&& val)
            : impl_(std::move(val.impl_))
        {
            val.impl_ = nullptr;
        }
        ~any()
        {
            delete impl_;
        }

        template<typename T>
        explicit any(T val)
        {
            impl_ = new any_handle_impl<typename type_wrapper<T>::value_type>(val);
        }

        template <typename T>
        typename type_wrapper<T>::reference cast() 
        {
            if (typeid(*impl_) != typeid(any_handle_impl<typename type_wrapper<T>::value_type>))
            {
                JSONCONS_THROW_EXCEPTION(std::exception,"Bad any cast");
            }
            return static_cast<any_handle_impl<typename type_wrapper<T>::value_type>&>(*impl_).value_;
        }

        template <typename T>
        typename type_wrapper<T>::const_reference cast() const
        {
            if (typeid(*impl_) != typeid(any_handle_impl<typename type_wrapper<T>::value_type>))
            {
                JSONCONS_THROW_EXCEPTION(std::exception,"Bad any cast");
            }
            return static_cast<any_handle_impl<typename type_wrapper<T>::value_type>&>(*impl_).value_;
        }

        any& operator=(any rhs)
        {
            std::swap(impl_,rhs.impl_);
            return *this;
        }

        void to_stream(basic_json_output_handler<Char>& os) const 
        {
            impl_->to_stream(os);
        }

        class any_handle
        {
        public:
            virtual ~any_handle()
            {
            }

            virtual any_handle* clone() const = 0;

            virtual void to_stream(basic_json_output_handler<Char>& os) const = 0;
        };

        template <class T>
        class any_handle_impl : public any_handle
        {
        public:
            any_handle_impl(T value)
                : value_(value)
            {
            }

            virtual any_handle* clone() const
            {
                return new any_handle_impl<T>(value_);
            }

            virtual void to_stream(basic_json_output_handler<Char>& os) const
            {
                serialize(os,value_);
            }

            T value_;
        };

        any_handle* impl_;
    };

    struct variant
    {
        struct string_data
        {
			const Char* c_str() const { return p; }
			size_t length() const { return length_; }

			bool operator==(const string_data& rhs) const
			{
				return length() == rhs.length() ? std::char_traits<Char>::compare(c_str(), rhs.c_str(), length()) == 0 : false;
			}

            string_data()
				: p(nullptr), length_(0)
			{
			}
			Char* p;
			size_t length_;
        private:
			string_data(const string_data&);
			string_data& operator=(const string_data&);
        };

        struct string_dataA
        {
        	string_data data;
        	Char c[1];
        };

        static string_data* make_string_data()

        {
            size_t length = 0;
            typedef typename std::aligned_storage<sizeof(string_dataA), JSONCONS_ALIGNOF(string_dataA)>::type storage_type;

            char* storage = new char[sizeof(storage_type) + length*sizeof(Char)];
            string_data* ps = new(storage)string_data();
            auto psa = reinterpret_cast<string_dataA*>(storage); 

            ps->p = new(&psa->c)Char[length + 1];
            ps->p[length] = 0;
            ps->length_ = length;
            return ps;
        }

        static string_data* make_string_data(const Char* s, size_t length)

        {
            typedef typename std::aligned_storage<sizeof(string_dataA), JSONCONS_ALIGNOF(string_dataA)>::type storage_type;

            char* storage = new char[sizeof(storage_type) + length*sizeof(Char)];
            string_data* ps = new(storage)string_data();
            auto psa = reinterpret_cast<string_dataA*>(storage); 

            ps->p = new(&psa->c)Char[length + 1];
            memcpy(ps->p, s, length*sizeof(Char));
            ps->p[length] = 0;
            ps->length_ = length;
            return ps;
        }

        static void destroy_string_data(string_data* p)
        {
            ::operator delete(reinterpret_cast<void*>(p));
        }

        static const size_t small_string_capacity = (sizeof(int64_t)/sizeof(Char)) - 1;

        variant()
            : type_(value_types::empty_object_t)
        {
        }
		
        explicit variant(variant&& rhs)
            : type_(value_types::null_t)
        {
            swap(rhs);
        }

        explicit variant(const variant& var)
            : type_(var.type_)
        {
            switch (var.type_)
            {
            case value_types::null_t:
            case value_types::empty_object_t:
                break;
            case value_types::double_t:
                value_.float_value_ = var.value_.float_value_;
                break;
            case value_types::integer_t:
                value_.integer_value_ = var.value_.integer_value_;
                break;
            case value_types::uinteger_t:
                value_.uinteger_value_ = var.value_.uinteger_value_;
                break;
            case value_types::bool_t:
                value_.bool_value_ = var.value_.bool_value_;
                break;
            case value_types::small_string_t:
                small_string_length_ = var.small_string_length_;
                std::memcpy(value_.small_string_value_,var.value_.small_string_value_,var.small_string_length_*sizeof(Char));
                value_.small_string_value_[small_string_length_] = 0;
                break;
            case value_types::string_t:
                value_.string_value_ = make_string_data(var.value_.string_value_->c_str(),var.value_.string_value_->length());
                break;
            case value_types::array_t:
                value_.array_ = new json_array<basic_json<Char,Alloc>,Alloc>(*(var.value_.array_));
                break;
            case value_types::object_t:
                value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>(*(var.value_.object_));
                break;
            case value_types::any_t:
                value_.any_value_ = new any(*(var.value_.any_value_));
                break;
            default:
                // throw
                break;
            }
        }

        variant(const json_object<basic_json<Char,Alloc>,Alloc>& val)
            : type_(value_types::object_t)
        {
            value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>(val);
        }

        variant(json_object<basic_json<Char,Alloc>,Alloc>&& val)
            : type_(value_types::object_t)
        {
            value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>(std::move(val));
        }

        variant(const json_array<basic_json<Char,Alloc>,Alloc>& val)
            : type_(value_types::array_t)
        {
            value_.array_ = new json_array<basic_json<Char,Alloc>,Alloc>(val);
        }

        variant(json_array<basic_json<Char,Alloc>,Alloc>&& val)
            : type_(value_types::array_t)
        {
            value_.array_ = new json_array<basic_json<Char,Alloc>,Alloc>(std::move(val));
        }

        variant(value_types::value_types_t type, size_t size)
            : type_(type)
        {
            switch (type)
            {
            case value_types::null_t:
            case value_types::empty_object_t:
                break;
            case value_types::double_t:
                break;
            case value_types::integer_t:
                break;
            case value_types::uinteger_t:
                break;
            case value_types::bool_t:
                break;
            case value_types::small_string_t:
                small_string_length_ = 0;
                break;
            case value_types::string_t:
                value_.string_value_ = make_string_data();
                break;
            case value_types::array_t:
                value_.array_ = new json_array<basic_json<Char,Alloc>,Alloc>(size);
                break;
            case value_types::object_t:
                value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
                break;
            case value_types::any_t:
                value_.any_value_ = new any();
                break;
            default:
                // throw
                break;
            }
        }

        explicit variant(const any& var)
            : type_(value_types::any_t)
        {
            value_.any_value_ = new any(var);
        }

        explicit variant(jsoncons::null_type)
            : type_(value_types::null_t)
        {
        }

        explicit variant(bool val)
            : type_(value_types::bool_t)
        {
            value_.bool_value_ = val;
        }

        explicit variant(double val)
            : type_(value_types::double_t)
        {
            value_.float_value_ = val;
        }

        explicit variant(int64_t val)
            : type_(value_types::integer_t)
        {
            value_.integer_value_ = val;
        }

        explicit variant(uint64_t val)
            : type_(value_types::uinteger_t)
        {
            value_.uinteger_value_ = val;
        }

        explicit variant(const std::basic_string<Char>& s)
        {
            if (s.length() > variant::small_string_capacity)
            {
                type_ = value_types::string_t;
                value_.string_value_ = make_string_data(s.c_str(),s.length());
            }
            else
            {
                type_ = value_types::small_string_t;
                small_string_length_ = (unsigned char)s.length();
                std::memcpy(value_.small_string_value_,s.c_str(),s.length()*sizeof(Char));
                value_.small_string_value_[small_string_length_] = 0;
            }
        }

        explicit variant(const Char* s)
        {
            size_t length = std::char_traits<Char>::length(s);
            if (length > variant::small_string_capacity)
            {
                type_ = value_types::string_t;
                value_.string_value_ = make_string_data(s,std::char_traits<Char>::length(s));
            }
            else
            {
                type_ = value_types::small_string_t;
                small_string_length_ = (unsigned char)length;
                std::memcpy(value_.small_string_value_,s,length*sizeof(Char));
                value_.small_string_value_[small_string_length_] = 0;
            }
        }

        explicit variant(const Char* s, size_t length)
        {
            if (length > variant::small_string_capacity)
            {
                type_ = value_types::string_t;
                value_.string_value_ = make_string_data(s,length);
            }
            else
            {
                type_ = value_types::small_string_t;
                small_string_length_ = (unsigned char)length;
                std::memcpy(value_.small_string_value_,s,length*sizeof(Char));
                value_.small_string_value_[small_string_length_] = 0;
            }
        }

        template<class InputIterator>
        variant(InputIterator first, InputIterator last)
            : type_(value_types::array_t)
        {
            value_.array_ = new json_array<basic_json<Char,Alloc>,Alloc>(first, last);
        }

        ~variant()
        {
            switch (type_)
            {
            case value_types::string_t:
                destroy_string_data(value_.string_value_);
                break;
            case value_types::array_t:
                delete value_.array_;
                break;
            case value_types::object_t:
                delete value_.object_;
                break;
            case value_types::any_t:
                delete value_.any_value_;
                break;
            default:
                break; 
            }
        }

        void destroy()
        {
            switch (type_)
            {
            case value_types::string_t:
                destroy_string_data(value_.string_value_);
                break;
            case value_types::array_t:
                delete value_.array_;
                break;
            case value_types::object_t:
                delete value_.object_;
                break;
            case value_types::any_t:
                delete value_.any_value_;
                break;
            default:
                break; 
            }
        }

        variant& operator=(const variant& val)
        {
            if (this != &val)
            {
                switch (type_)
                {
                case value_types::null_t:
                case value_types::bool_t:
                case value_types::empty_object_t:
                case value_types::small_string_t:
                case value_types::integer_t:
                case value_types::uinteger_t:
                case value_types::double_t:
                    switch (val.type_)
                    {
                    case value_types::null_t:
                    case value_types::bool_t:
                    case value_types::empty_object_t:
                    case value_types::small_string_t:
                    case value_types::integer_t:
                    case value_types::uinteger_t:
                    case value_types::double_t:
                        type_ = val.type_;
                        small_string_length_ = val.small_string_length_;
                        value_ = val.value_;
                        break;
                    default:
                        swap(variant(val));
                        break;
                    }
                    break;
                default:
                    {
                        swap(variant(val));
                    }
                    break;
                }
            }
            return *this;
        }

        variant& operator=(variant&& val)
        {
            if (this != &val)
            {
                val.swap(*this);
            }
            return *this;
        }

        void assign(const json_object<basic_json<Char,Alloc>,Alloc>& val)
        {
			destroy();
			type_ = value_types::object_t;
			value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>(val);
		}

        void assign(json_object<basic_json<Char,Alloc>,Alloc>&& val)
        {
			switch (type_)
			{
			case value_types::object_t:
				value_.object_->swap(val);
				break;
			default:
				destroy();
				type_ = value_types::object_t;
				value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>(std::move(val));
				break;
			}
		}

        void assign(const json_array<basic_json<Char,Alloc>,Alloc>& val)
        {
            destroy();
            type_ = value_types::array_t;
            value_.array_ = new json_array<basic_json<Char,Alloc>,Alloc>(val);
        }

        void assign(json_array<basic_json<Char,Alloc>,Alloc>&& val)
        {
			switch (type_)
			{
			case value_types::array_t:
				value_.array_->swap(val);
				break;
			default:
				destroy();
				type_ = value_types::array_t;
				value_.array_ = new json_array<basic_json<Char,Alloc>,Alloc>(std::move(val));
				break;
			}
		}

        void assign(const std::basic_string<Char>& s)
        {
            switch (type_)
            {
            case value_types::null_t:
            case value_types::bool_t:
            case value_types::empty_object_t:
            case value_types::small_string_t:
            case value_types::integer_t:
            case value_types::uinteger_t:
            case value_types::double_t:
                if (s.length() > variant::small_string_capacity)
                {
                    type_ = value_types::string_t;
                    value_.string_value_ = make_string_data(s.c_str(),s.length());
                }
                else
                {
                    type_ = value_types::small_string_t;
                    small_string_length_ = (unsigned char)s.length();
                    std::memcpy(value_.small_string_value_,s.c_str(),s.length()*sizeof(Char));
                    value_.small_string_value_[small_string_length_] = 0;
                }
                break;
            default:
                variant(s).swap(*this);
                break;
            }
        }

        void assign_string(const Char* s, size_t length)
        {
            switch (type_)
            {
            case value_types::null_t:
            case value_types::bool_t:
            case value_types::empty_object_t:
            case value_types::small_string_t:
            case value_types::integer_t:
            case value_types::uinteger_t:
            case value_types::double_t:
				{
					if (length > variant::small_string_capacity)
					{
						type_ = value_types::string_t;
						value_.string_value_ = make_string_data(s,length);
					}
					else
					{
						type_ = value_types::small_string_t;
						small_string_length_ = (unsigned char)length;
						std::memcpy(value_.small_string_value_,s,length*sizeof(Char));
                        value_.small_string_value_[small_string_length_] = 0;
					}
				}
                break;
            default:
                variant(s,length).swap(*this);
                break;
            }
        }

        void assign(int64_t val)
        {
            switch (type_)
            {
            case value_types::null_t:
            case value_types::bool_t:
            case value_types::empty_object_t:
            case value_types::small_string_t:
            case value_types::integer_t:
            case value_types::uinteger_t:
            case value_types::double_t:
                type_ = value_types::integer_t;
                value_.integer_value_ = val;
                break;
            default:
                variant(val).swap(*this);
                break;
            }
        }

        void assign(uint64_t val)
        {
            switch (type_)
            {
            case value_types::null_t:
            case value_types::bool_t:
            case value_types::empty_object_t:
            case value_types::small_string_t:
            case value_types::integer_t:
            case value_types::uinteger_t:
            case value_types::double_t:
                type_ = value_types::uinteger_t;
                value_.uinteger_value_ = val;
                break;
            default:
                variant(val).swap(*this);
                break;
            }
        }

        void assign(double val)
        {
            switch (type_)
            {
            case value_types::null_t:
            case value_types::bool_t:
            case value_types::empty_object_t:
            case value_types::small_string_t:
            case value_types::integer_t:
            case value_types::uinteger_t:
            case value_types::double_t:
                type_ = value_types::double_t;
                value_.float_value_ = val;
                break;
            default:
                variant(val).swap(*this);
                break;
            }
        }

        void assign(bool val)
        {
            switch (type_)
            {
            case value_types::null_t:
            case value_types::bool_t:
            case value_types::empty_object_t:
            case value_types::small_string_t:
            case value_types::integer_t:
            case value_types::uinteger_t:
            case value_types::double_t:
                type_ = value_types::bool_t;
                value_.bool_value_ = val;
                break;
            default:
                variant(val).swap(*this);
                break;
            }
        }

        void assign(null_type)
        {
            switch (type_)
            {
            case value_types::null_t:
            case value_types::bool_t:
            case value_types::empty_object_t:
            case value_types::small_string_t:
            case value_types::integer_t:
            case value_types::uinteger_t:
            case value_types::double_t:
                type_ = value_types::null_t;
                break;
            default:
                variant(null_type()).swap(*this);
                break;
            }
        }

        void assign(const any& rhs)
        {
            switch (type_)
            {
            case value_types::null_t:
            case value_types::bool_t:
            case value_types::empty_object_t:
            case value_types::small_string_t:
            case value_types::integer_t:
            case value_types::uinteger_t:
            case value_types::double_t:
                type_ = value_types::any_t;
                value_.any_value_ = new any(rhs);
                break;
            default:
                variant(rhs).swap(*this);
                break;
            }
        }

        bool operator!=(const variant& rhs) const
        {
            return !(*this == rhs);
        }

        bool operator==(const variant& rhs) const
        {
            if (is_number() & rhs.is_number())
            {
                switch (type_)
                {
                case value_types::integer_t:
                    switch (rhs.type_)
                    {
                    case value_types::integer_t:
                        return value_.integer_value_ == rhs.value_.integer_value_;
                    case value_types::uinteger_t:
                        return value_.integer_value_ == rhs.value_.uinteger_value_;
                    case value_types::double_t:
                        return value_.integer_value_ == rhs.value_.float_value_;
                    }
                    break;
                case value_types::uinteger_t:
                    switch (rhs.type_)
                    {
                    case value_types::integer_t:
                        return value_.uinteger_value_ == rhs.value_.integer_value_;
                    case value_types::uinteger_t:
                        return value_.uinteger_value_ == rhs.value_.uinteger_value_;
                    case value_types::double_t:
                        return value_.uinteger_value_ == rhs.value_.float_value_;
                    }
                    break;
                case value_types::double_t:
                    switch (rhs.type_)
                    {
                    case value_types::integer_t:
                        return value_.float_value_ == rhs.value_.integer_value_;
                    case value_types::uinteger_t:
                        return value_.float_value_ == rhs.value_.uinteger_value_;
                    case value_types::double_t:
                        return value_.float_value_ == rhs.value_.float_value_;
                    }
                    break;
                }
            }

            if (rhs.type_ != type_)
            {
                return false;
            }
            switch (type_)
            {
            case value_types::bool_t:
                return value_.bool_value_ == rhs.value_.bool_value_;
            case value_types::null_t:
            case value_types::empty_object_t:
                return true;
            case value_types::small_string_t:
                return small_string_length_ == rhs.small_string_length_ ? std::char_traits<Char>::compare(value_.small_string_value_,rhs.value_.small_string_value_,small_string_length_) == 0 : false;
            case value_types::string_t:
                return *(value_.string_value_) == *(rhs.value_.string_value_);
            case value_types::array_t:
                return *(value_.array_) == *(rhs.value_.array_);
                break;
            case value_types::object_t:
                return *(value_.object_) == *(rhs.value_.object_);
                break;
            case value_types::any_t:
                break;
            default:
                // throw
                break;
            }
            return false;
        }

        bool is_null() const JSONCONS_NOEXCEPT
        {
            return type_ == value_types::null_t;
        }

        bool is_bool() const JSONCONS_NOEXCEPT
        {
            return type_ == value_types::bool_t;
        }

        // Deprecated
        bool is_empty() const JSONCONS_NOEXCEPT
        {
            return empty();
        }

        bool empty() const JSONCONS_NOEXCEPT
        {
            switch (type_)
            {
            case value_types::small_string_t:
                return small_string_length_ == 0;
            case value_types::string_t:
                return value_.string_value_->length() == 0;
            case value_types::array_t:
                return value_.array_->size() == 0;
            case value_types::empty_object_t:
                return true;
            case value_types::object_t:
                return value_.object_->size() == 0;
            default:
                return false;
            }
        }

        bool is_string() const JSONCONS_NOEXCEPT
        {
            return (type_ == value_types::string_t) | (type_ == value_types::small_string_t);
        }

        bool is_number() const JSONCONS_NOEXCEPT
        {
            return type_ == value_types::double_t || type_ == value_types::integer_t || type_ == value_types::uinteger_t;
        }

        void swap(variant& var)
        {
            using std::swap;

            swap(type_,var.type_);
            swap(small_string_length_,var.small_string_length_);
            swap(value_,var.value_);
        }

        value_types::value_types_t type_;
        unsigned char small_string_length_;
        union
        {
            double float_value_;
            int64_t integer_value_;
            uint64_t uinteger_value_;
            bool bool_value_;
            json_object<basic_json<Char,Alloc>,Alloc>* object_;
            json_array<basic_json<Char,Alloc>,Alloc>* array_;
            any* any_value_;
            string_data* string_value_;
            Char small_string_value_[sizeof(int64_t)/sizeof(Char)];
        } value_;
    };

    // Deprecated
    typedef any json_any_type;

    typedef json_object_member<basic_json<Char,Alloc>> member_type;
    typedef member_type name_value_pair;

    // Deprecated static data members
    static const basic_json<Char,Alloc> an_object;
    static const basic_json<Char,Alloc> an_array;
    static const basic_json<Char,Alloc> null;

    typedef typename json_object<basic_json<Char,Alloc>,Alloc>::iterator object_iterator;
    typedef typename json_object<basic_json<Char,Alloc>,Alloc>::const_iterator const_object_iterator;
    typedef typename json_array<basic_json<Char,Alloc>,Alloc>::iterator array_iterator;
    typedef typename json_array<basic_json<Char,Alloc>,Alloc>::const_iterator const_array_iterator;

    template <typename structure, bool is_const_iterator = true>
    class range 
    {
        typedef typename std::conditional<is_const_iterator, typename const structure&, typename structure&>::type structure_ref;
        typedef typename std::conditional<is_const_iterator, typename structure::const_iterator, typename structure::iterator>::type iterator;
        typedef typename structure::const_iterator const_iterator;
        structure_ref val_;

    public:
        range(structure_ref val)
            : val_(val)
        {
        }

    public:
        friend class basic_json<Char, Alloc>;

        iterator begin()
        {
            return val_.begin();
        }
        iterator end()
        {
            return val_.end();
        }
    };

    typedef range<object,false> object_range;
    typedef range<object,true> const_object_range;
    typedef range<array,false> array_range;
    typedef range<array,true> const_array_range;

    class object_key_proxy 
    {
    private:
        basic_json<Char,Alloc>& parent_;
        const std::basic_string<Char>& name_;

        object_key_proxy(); // noop
        object_key_proxy& operator = (const object_key_proxy& other); // noop

        object_key_proxy(basic_json<Char,Alloc>& parent, 
              const std::basic_string<Char>& name)
            : parent_(parent), name_(name)
        {
        }

        basic_json<Char,Alloc>& evaluate() 
        {
            return parent_.at(name_);
        }

        const basic_json<Char,Alloc>& evaluate() const
        {
            return parent_.at(name_);
        }
    public:

        friend class basic_json<Char,Alloc>;

        object_range members()
        {
            return evaluate().members();
        }

        const_object_range members() const
        {
            return evaluate().members();
        }

        array_range elements()
        {
            return evaluate().elements();
        }

        const_array_range elements() const
        {
            return evaluate().elements();
        }

        object_iterator begin_members()
        {
            return evaluate().begin_members();
        }

        const_object_iterator begin_members() const
        {
            return evaluate().begin_members();
        }

        object_iterator end_members()
        {
            return evaluate().end_members();
        }

        const_object_iterator end_members() const
        {
            return evaluate().end_members();
        }

        array_iterator begin_elements()
        {
            return evaluate().begin_elements();
        }

        const_array_iterator begin_elements() const
        {
            return evaluate().begin_elements();
        }

        array_iterator end_elements()
        {
            return evaluate().end_elements();
        }

        const_array_iterator end_elements() const
        {
            return evaluate().end_elements();
        }

        size_t size() const JSONCONS_NOEXCEPT
        {
            return evaluate().size();
        }

        value_types::value_types_t type() const
        {
            return evaluate().type();
        }

        // Deprecated
        bool has_member(const std::basic_string<Char>& name) const
        {
            return evaluate().has_member(name);
        }

        size_t count(const std::basic_string<Char>& name) const
        {
            return evaluate().count(name);
        }

        bool is_null() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_null();
        }

        // Deprecated
        bool is_empty() const JSONCONS_NOEXCEPT
        {
            return empty();
        }

        bool empty() const
        {
            return evaluate().empty();
        }

        size_t capacity() const
        {
            return evaluate().capacity();
        }

        void reserve(size_t n)
        {
            evaluate().reserve(n);
        }

        void resize(size_t n)
        {
            evaluate().resize(n);
        }

        template <typename T>
        void resize(size_t n, T val)
        {
            evaluate().resize(n,val);
        }

        void resize_array(size_t n)
        {
            evaluate().resize_array(n);
        }

        template <typename T>
        void resize_array(size_t n, T val)
        {
            evaluate().resize_array(n,val);
        }

        template<typename T>
        bool is() const
        {
            return evaluate().template is<T>();
        }

        bool is_string() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_string();
        }

        bool is_number() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_number();
        }

        // Deprecate
        bool is_numeric() const JSONCONS_NOEXCEPT
        {
            return is_number();
        }

        bool is_bool() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_bool();
        }

        bool is_object() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_object();
        }

        bool is_array() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_array();
        }
 
        bool is_any() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_any();
        }

        bool is_longlong() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_longlong();
        }

        bool is_integer() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_integer();
        }

        bool is_ulonglong() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_ulonglong();
        }

        bool is_uinteger() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_uinteger();
        }

        bool is_double() const JSONCONS_NOEXCEPT
        {
            return evaluate().is_double();
        }

        std::basic_string<Char> as_string() const
        {
            return evaluate().as_string();
        }

        std::basic_string<Char> as_string(const basic_output_format<Char>& format) const
        {
            return evaluate().as_string(format);
        }

        template<typename T>
        T as() const
        {
            return evaluate().template as<T>();
        }

        any& any_value()
        {
            return evaluate().any_value();
        }

        const any& any_value() const
        {
            return evaluate().any_value();
        }

        bool as_bool() const JSONCONS_NOEXCEPT
        {
            return evaluate().as_bool();
        }

        template <class T>
        std::vector<T> as_vector() const
        {
            return evaluate().template as_vector<T>();
        }

        double as_double() const
        {
            return evaluate().as_double();
        }

        int as_int() const
        {
            return evaluate().as_int();
        }

        unsigned int as_uint() const
        {
            return evaluate().as_uint();
        }

        long as_long() const
        {
            return evaluate().as_long();
        }

        unsigned long as_ulong() const
        {
            return evaluate().as_ulong();
        }

        long long as_longlong() const
        {
            return evaluate().as_longlong();
        }

        int64_t as_integer() const
        {
            return evaluate().as_integer();
        }

        unsigned long long as_ulonglong() const
        {
            return evaluate().as_ulonglong();
        }

        uint64_t as_uinteger() const
        {
            return evaluate().as_uinteger();
        }

        template <class T>
        const T& any_cast() const
        {
            return evaluate().template any_cast<T>();
        }
        // Returns a const reference to the custom data associated with name

        template <class T>
        T& any_cast() 
        {
            return evaluate().template any_cast<T>();
        }
        // Returns a reference to the custom data associated with name

        operator basic_json&()
        {
            return evaluate();
        }

        operator const basic_json&() const
        {
            return evaluate();
        }

        template <typename T>
        object_key_proxy& operator=(T val)
        {
            parent_.set(name_, val);
            return *this;
        }

        object_key_proxy& operator=(const basic_json& val)
        {
            parent_.set(name_, val);
            return *this;
        }

        object_key_proxy& operator=(basic_json<Char,Alloc>&& val)
        {
            parent_.set(name_, std::move(val));
            return *this;
        }

        bool operator==(const basic_json& val) const
        {
            return evaluate() == val;
        }

        bool operator!=(const basic_json& val) const
        {
            return evaluate() != val;
        }

        basic_json<Char,Alloc>& operator[](size_t i)
        {
            return evaluate()[i];
        }

        const basic_json<Char,Alloc>& operator[](size_t i) const
        {
            return evaluate()[i];
        }

        object_key_proxy operator[](const std::basic_string<Char>& name)
        {
            return object_key_proxy(evaluate(),name);
        }

        const basic_json<Char,Alloc>& operator[](const std::basic_string<Char>& name) const
        {
            return evaluate().at(name);
        }

        basic_json<Char,Alloc>& at(const std::basic_string<Char>& name)
        {
            return evaluate().at(name);
        }

        const basic_json<Char,Alloc>& at(const std::basic_string<Char>& name) const
        {
            return evaluate().at(name);
        }

        object_iterator find(const std::basic_string<Char>& name)
        {
            return evaluate().find(name);
        }

        const_object_iterator find(const std::basic_string<Char>& name) const
        {
            return evaluate().find(name);
        }

        object_iterator find(const Char* name)
        {
            return evaluate().find(name);
        }

        const_object_iterator find(const Char* name) const
        {
            return evaluate().find(name);
        }

        const basic_json<Char,Alloc>& get(const std::basic_string<Char>& name) const
        {
            return evaluate().get(name);
        }

        template <typename T>
        basic_json<Char,Alloc> get(const std::basic_string<Char>& name, const T& default_val) const
        {
            return evaluate().get(name,default_val);
        }

        void clear()
        {
            evaluate().clear();
        }
        // Remove all elements from an array or object

        void erase(object_iterator first, object_iterator last)
        {
            evaluate().erase(first, last);
        }
        // Remove a range of elements from an object 

        void erase(array_iterator first, array_iterator last)
        {
            evaluate().erase(first, last);
        }
        // Remove a range of elements from an array 

        void remove_range(size_t from_index, size_t to_index)
        {
            evaluate().remove_range(from_index, to_index);
        }
        // Remove a range of elements from an array 

        void erase(const std::basic_string<Char>& name)
        {
            evaluate().erase(name);
        }

        void remove(const std::basic_string<Char>& name)
        {
            evaluate().remove(name);
        }

        // Deprecated
        void remove_member(const std::basic_string<Char>& name)
        {
            evaluate().remove(name);
        }

       // Remove a member from an object 

        void set(const std::basic_string<Char>& name, const basic_json<Char,Alloc>& value)
        {
            evaluate().set(name,value);
        }

        void set(std::basic_string<Char>&& name, const basic_json<Char,Alloc>& value)

        {
            evaluate().set(std::move(name),value);
        }

        void set(const std::basic_string<Char>& name, basic_json<Char,Alloc>&& value)

        {
            evaluate().set(name,std::move(value));
        }

        void set(std::basic_string<Char>&& name, basic_json<Char,Alloc>&& value)

        {
            evaluate().set(std::move(name),std::move(value));
        }

        object_iterator set(object_iterator hint, const std::basic_string<Char>& name, const basic_json<Char,Alloc>& value)
        {
            return evaluate().set(hint, name,value);
        }

        object_iterator set(object_iterator hint, std::basic_string<Char>&& name, const basic_json<Char,Alloc>& value)

        {
            return evaluate().set(hint, std::move(name),value);
        }

        object_iterator set(object_iterator hint, const std::basic_string<Char>& name, basic_json<Char,Alloc>&& value)

        {
            return evaluate().set(hint, name,std::move(value));
        }

        object_iterator set(object_iterator hint, std::basic_string<Char>&& name, basic_json<Char,Alloc>&& value)

        {
            return evaluate().set(hint, std::move(name),std::move(value));
        }

        void add(basic_json<Char,Alloc>&& value)
        {
            evaluate().add(std::move(value));
        }

        void add(const basic_json<Char,Alloc>& value)
        {
            evaluate().add(value);
        }

        void add(size_t index, const basic_json<Char,Alloc>& value)
        {
            evaluate().add(index, value);
        }

        void add(size_t index, basic_json<Char,Alloc>&& value)
        {
            evaluate().add(index, std::move(value));
        }

        void add(array_iterator pos, const basic_json<Char,Alloc>& value)
        {
            evaluate().add(pos, value);
        }

        void add(array_iterator pos, basic_json<Char,Alloc>&& value)
        {
            evaluate().add(pos, std::move(value));
        }

        std::basic_string<Char> to_string() const
        {
            return evaluate().to_string();
        }

        std::basic_string<Char> to_string(const basic_output_format<Char>& format) const
        {
            return evaluate().to_string(format);
        }

        void to_stream(std::basic_ostream<Char>& os) const
        {
            evaluate().to_stream(os);
        }

        void to_stream(std::basic_ostream<Char>& os, const basic_output_format<Char>& format) const
        {
            evaluate().to_stream(os,format);
        }

        void to_stream(std::basic_ostream<Char>& os, const basic_output_format<Char>& format, bool indenting) const
        {
            evaluate().to_stream(os,format,indenting);
        }

        void swap(basic_json<Char,Alloc>& val)
        {
            parent_.swap(val);
        }

        friend std::basic_ostream<Char>& operator<<(std::basic_ostream<Char>& os, const object_key_proxy& o)
        {
            o.to_stream(os);
            return os;
        }
    };

    static basic_json parse_stream(std::basic_istream<Char>& is);
    static basic_json parse_stream(std::basic_istream<Char>& is, basic_parse_error_handler<Char>& err_handler);

    // Deprecated
    static basic_json parse(std::basic_istream<Char>& is)
    {
        parse_stream(is);
    }
    static basic_json parse(std::basic_istream<Char>& is, basic_parse_error_handler<Char>& err_handler)
    {
        parse_stream(is,err_handler);
    }

    static basic_json parse(const std::basic_string<Char>& s);

    static basic_json parse(const std::basic_string<Char>& s, basic_parse_error_handler<Char>& err_handler);

    static basic_json parse_string(const std::basic_string<Char>& s);

    static basic_json parse_string(const std::basic_string<Char>& s, basic_parse_error_handler<Char>& err_handler);

    static basic_json parse_file(const std::string& s);

    static basic_json parse_file(const std::string& s, basic_parse_error_handler<Char>& err_handler);

    static basic_json<Char,Alloc> make_array()
    {
        return typename basic_json<Char,Alloc>::array();
    }

    static basic_json<Char,Alloc> make_array(size_t n)
    {
        basic_json<Char,Alloc> val = make_array();
        val.resize(n);
        return val;
    }

    template <typename T>
    static basic_json<Char,Alloc> make_array(size_t n, T val)
    {
        basic_json<Char,Alloc> a = make_array();
        a.resize(n,val);
        return a;
    }

    template<int size>
    static typename std::enable_if<size==1,basic_json>::type make_array()
    {
        return build_array<Char,Alloc,size>()();
    }

    template<size_t size>
    static typename std::enable_if<size==1,basic_json>::type make_array(size_t n)
    {
        return build_array<Char,Alloc,size>()(n);
    }
    template<size_t size,typename T>
    static typename std::enable_if<size==1,basic_json>::type make_array(size_t n, T val)
    {
        return build_array<Char,Alloc,size>()(n, val);
    }
    template<size_t size>
    static typename std::enable_if<size==2,basic_json>::type make_array(size_t m, size_t n)
    {
        return build_array<Char,Alloc,size>()(m, n);
    }
    template<size_t size,typename T>
    static typename std::enable_if<size==2,basic_json>::type make_array(size_t m, size_t n, T val)
    {
        return build_array<Char,Alloc,size>()(m, n, val);
    }
    template<size_t size>
    static typename std::enable_if<size==3,basic_json>::type make_array(size_t m, size_t n, size_t k)
    {
        return build_array<Char,Alloc,size>()(m, n, k);
    }
    template<size_t size,typename T>
    static typename std::enable_if<size==3,basic_json>::type make_array(size_t m, size_t n, size_t k, T val)
    {
        return build_array<Char,Alloc,size>()(m, n, k, val);
    }

    basic_json()
    {
    }

    basic_json(const basic_json<Char, Alloc>& val)
        : var_(val.var_)
    {
    }

    basic_json(basic_json<Char,Alloc>&& other)
        : var_(std::move(other.var_))
    {
    }

    basic_json(const json_array<basic_json<Char,Alloc>,Alloc>& val)
        : var_(val)
    {
    }

    basic_json(json_array<basic_json<Char,Alloc>,Alloc>&& other)
        : var_(std::move(other))
    {
    }

    basic_json(json_object<basic_json<Char,Alloc>,Alloc>&& other)
        : var_(std::move(other))
    {
    }

    basic_json(const object_key_proxy& proxy)
        : var_(proxy.evaluate().var_)
    {
    }

    template <typename T>
    basic_json(T val)
        : var_()
    {
        json_type_traits<Char,Alloc,T>::assign(*this,val);
    }

    basic_json(const Char *s, size_t length)
        : var_(s, length)
    {
    }

    basic_json(value_types::value_types_t type, size_t size)
        : var_(type,size)
    {
    }

    template<class InputIterator>
    basic_json(InputIterator first, InputIterator last)
        : var_(first,last)
    {
    }

    ~basic_json()
    {
    }

    object_iterator begin_members();

    const_object_iterator begin_members() const;

    object_iterator end_members();

    const_object_iterator end_members() const;

    array_iterator begin_elements();

    const_array_iterator begin_elements() const;

    array_iterator end_elements();

    const_array_iterator end_elements() const;

    basic_json& operator=(const basic_json<Char,Alloc>& rhs)
    {
        var_ = rhs.var_;
        return *this;
    }

    basic_json& operator=(basic_json<Char,Alloc>&& rhs)
    {
        if (this != &rhs)
        {
            var_ = std::move(rhs.var_);
        }
        return *this;
    }

    template <class T>
    basic_json<Char, Alloc>& operator=(T val)
    {
        json_type_traits<Char,Alloc,T>::assign(*this,val);
        return *this;
    }

    bool operator!=(const basic_json<Char,Alloc>& rhs) const;

    bool operator==(const basic_json<Char,Alloc>& rhs) const;

    size_t size() const JSONCONS_NOEXCEPT
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return 0;
        case value_types::object_t:
            return var_.value_.object_->size();
        case value_types::array_t:
            return var_.value_.array_->size();
        default:
            return 0;
        }
    }

    basic_json<Char,Alloc>& operator[](size_t i)
    {
        return at(i);
    }

    const basic_json<Char,Alloc>& operator[](size_t i) const
    {
        return at(i);
    }

    object_key_proxy operator[](const std::basic_string<Char>& name)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
        case value_types::object_t:
            return object_key_proxy(*this, name);
            break;
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not a long long");
            break;
        }
    }

    const basic_json<Char,Alloc>& operator[](const std::basic_string<Char>& name) const
    {
        return at(name);
    }

    std::basic_string<Char> to_string() const;

    std::basic_string<Char> to_string(const basic_output_format<Char>& format) const;

    void to_stream(std::basic_ostream<Char>& os) const;

    void to_stream(std::basic_ostream<Char>& os, const basic_output_format<Char>& format) const;

    void to_stream(std::basic_ostream<Char>& os, const basic_output_format<Char>& format, bool indenting) const;

    bool is_null() const JSONCONS_NOEXCEPT
    {
        return var_.is_null();
    }

    // Deprecated
    bool has_member(const std::basic_string<Char>& name) const;

    size_t count(const std::basic_string<Char>& name) const
    {
        switch (var_.type_)
        {
        case value_types::object_t:
            {
                auto it = var_.value_.object_->find(name);
                if (it == end_members())
                {
                    return 0;
                }
                size_t count = 0;
                while (it != end_members() && it->name() == name)
                {
                    ++count;
                    ++it;
                }
                return count;
            }
            break;
        default:
            return 0;
        }
    }

    template<typename T>
    bool is() const
    {
        return json_type_traits<Char,Alloc,T>::is(*this);
    }

    bool is_string() const JSONCONS_NOEXCEPT
    {
        return var_.is_string();
    }


    bool is_bool() const JSONCONS_NOEXCEPT
    {
        return var_.is_bool();
    }

    bool is_object() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::object_t || var_.type_ == value_types::empty_object_t;
    }

    bool is_array() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::array_t;
    }

    bool is_any() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::any_t;
    }

    bool is_longlong() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::integer_t;
    }

    bool is_integer() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::integer_t;
    }

    bool is_ulonglong() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::uinteger_t;
    }

    bool is_uinteger() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::uinteger_t;
    }

    bool is_double() const JSONCONS_NOEXCEPT
    {
        return var_.type_ == value_types::double_t;
    }

    bool is_number() const JSONCONS_NOEXCEPT
    {
        return var_.is_number();
    }

    // Deprecated 
    bool is_numeric() const JSONCONS_NOEXCEPT
    {
        return is_number();
    }

    // Deprecated
    bool is_empty() const JSONCONS_NOEXCEPT
    {
        return empty();
    }

    bool empty() const JSONCONS_NOEXCEPT
    {
        return var_.empty();
    }

    size_t capacity() const
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return var_.value_.array_->capacity();
        case value_types::object_t:
            return var_.value_.object_->capacity();
        default:
            return 0;
        }
    }

    void reserve(size_t n)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_->reserve(n);
            break;
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            var_.value_.object_->reserve(n);
            break;
        }
    }

    void resize(size_t n)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_->resize(n);
            break;
        }
    }

    void resize_array(size_t n)
    {
        resize(n);
    }

    template <typename T>
    void resize(size_t n, T val)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_->resize(n, val);
            break;
        }
    }

    template <typename T>
    void resize_array(size_t n, T val)
    {
        resize(n,val);
    }

    template<typename T>
    T as() const
    {
        return json_type_traits<Char,Alloc,T>::as(*this);
    }

    bool as_bool() const JSONCONS_NOEXCEPT
    {
        switch (var_.type_)
        {
        case value_types::null_t:
        case value_types::empty_object_t:
            return false;
        case value_types::bool_t:
            return var_.value_.bool_value_;
        case value_types::double_t:
            return var_.value_.float_value_ != 0.0;
        case value_types::integer_t:
            return var_.value_.integer_value_ != 0;
        case value_types::uinteger_t:
            return var_.value_.uinteger_value_ != 0;
        case value_types::small_string_t:
            return var_.small_string_length_ != 0;
        case value_types::string_t:
            return var_.value_.string_value_->length() != 0;
        case value_types::array_t:
            return var_.value_.array_->size() != 0;
        case value_types::object_t:
            return var_.value_.object_->size() != 0;
        case value_types::any_t:
            return true;
        default:
            return false;
        }
    }

    long long as_longlong() const
    {
        return as_integer();
    }

    unsigned long long as_ulonglong() const
    {
        return as_uinteger();
    }

    int64_t as_integer() const
    {
        switch (var_.type_)
        {
        case value_types::double_t:
            return static_cast<long long>(var_.value_.float_value_);
        case value_types::integer_t:
            return static_cast<long long>(var_.value_.integer_value_);
        case value_types::uinteger_t:
            return static_cast<long long>(var_.value_.uinteger_value_);
        case value_types::bool_t:
            return var_.value_.bool_value_ ? 1 : 0;
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not a long long");
        }
    }

    uint64_t as_uinteger() const
    {
        switch (var_.type_)
        {
        case value_types::double_t:
            return static_cast<unsigned long long>(var_.value_.float_value_);
        case value_types::integer_t:
            return static_cast<unsigned long long>(var_.value_.integer_value_);
        case value_types::uinteger_t:
            return static_cast<unsigned long long>(var_.value_.uinteger_value_);
        case value_types::bool_t:
            return var_.value_.bool_value_ ? 1 : 0;
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not a unsigned long long");
        }
    }

    double as_double() const
    {
        switch (var_.type_)
        {
        case value_types::double_t:
            return var_.value_.float_value_;
        case value_types::integer_t:
            return static_cast<double>(var_.value_.integer_value_);
        case value_types::uinteger_t:
            return static_cast<double>(var_.value_.uinteger_value_);
        case value_types::null_t:
            return std::numeric_limits<double>::quiet_NaN();
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not a double");
        }
    }

    // Deprecated

    int as_int() const;

    unsigned int as_uint() const;

    long as_long() const;

    unsigned long as_ulong() const;

    std::basic_string<Char> as_string() const
    {
        switch (var_.type_)
        {
        case value_types::small_string_t:
            return std::basic_string<Char>(var_.value_.small_string_value_,var_.small_string_length_);
        case value_types::string_t:
            return std::basic_string<Char>(var_.value_.string_value_->c_str(),var_.value_.string_value_->length());
        default:
            return to_string();
        }
    }

    std::basic_string<Char> as_string(const basic_output_format<Char>& format) const
    {
        switch (var_.type_)
        {
        case value_types::small_string_t:
            return std::basic_string<Char>(var_.value_.small_string_value_,var_.small_string_length_);
        case value_types::string_t:
            return std::basic_string<Char>(var_.value_.string_value_->c_str(),var_.value_.string_value_->length());
        default:
            return to_string(format);
        }
    }

    const Char* as_cstring() const;

    any& any_value();

    const any& any_value() const;

    basic_json<Char, Alloc>& at(const std::basic_string<Char>& name)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            JSONCONS_THROW_EXCEPTION_1(std::out_of_range,"%s not found", name);
        case value_types::object_t:
            return var_.value_.object_->at(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    const basic_json<Char, Alloc>& at(const std::basic_string<Char>& name) const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            JSONCONS_THROW_EXCEPTION_1(std::out_of_range,"%s not found", name);
        case value_types::object_t:
            return var_.value_.object_->at(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    basic_json<Char, Alloc>& at(size_t i)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            if (i >= var_.value_.array_->size())
            {
                JSONCONS_THROW_EXCEPTION(std::out_of_range,"Invalid array subscript");
            }
            return var_.value_.array_->at(i);
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Index on non-array value not supported");
        }
    }

    const basic_json<Char, Alloc>& at(size_t i) const
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            if (i >= var_.value_.array_->size())
            {
                JSONCONS_THROW_EXCEPTION(std::out_of_range,"Invalid array subscript");
            }
            return var_.value_.array_->at(i);
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Index on non-array value not supported");
        }
    }

    object_iterator find(const std::basic_string<Char>& name)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return end_members();
        case value_types::object_t:
            return var_.value_.object_->find(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    const_object_iterator find(const std::basic_string<Char>& name) const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return end_members();
        case value_types::object_t:
            return var_.value_.object_->find(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    object_iterator find(const Char* name)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return end_members();
        case value_types::object_t:
            return var_.value_.object_->find(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    const_object_iterator find(const Char* name) const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return end_members();
        case value_types::object_t:
            return var_.value_.object_->find(name);
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to get %s from a value that is not an object", name);
            }
        }
    }

    const basic_json<Char,Alloc>& get(const std::basic_string<Char>& name) const;

    template <typename T>
    basic_json<Char,Alloc> get(const std::basic_string<Char>& name, const T& default_val) const;

    // Modifiers

    void clear();
    // Remove all elements from an array or object

    void erase(object_iterator first, object_iterator last)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            break;
        case value_types::object_t:
            var_.value_.object_->erase(first, last);
            break;
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not an object");
            break;
        }
    }

    void erase(array_iterator first, array_iterator last)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_->erase(first, last);
            break;
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not an array");
            break;
        }
    }

    // Removes all elements from an array value whose index is between from_index, inclusive, and to_index, exclusive.

    void erase(const std::basic_string<Char>& name)
    {
        switch (var_.type_)
        {
        case value_types::object_t:
            var_.value_.object_->remove(name);
            break;
        default:
            JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object", name);
            break;
        }
    }
    // Removes a member from an object value

    void remove_range(size_t from_index, size_t to_index)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_->remove_range(from_index, to_index);
            break;
        default:
            break;
        }
    }
    // Removes all elements from an array value whose index is between from_index, inclusive, and to_index, exclusive.

    void remove(const std::basic_string<Char>& name)
    {
        erase(name);
    }
    // Removes a member from an object value

    // Deprecated
    void remove_member(const std::basic_string<Char>& name)
    {
        erase(name);
    }
    // Removes a member from an object value

    void set(const std::basic_string<Char>& name, const basic_json<Char, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            var_.value_.object_->set(name, value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object", name);
            }
        }
    }

    void set(std::basic_string<Char>&& name, const basic_json<Char, Alloc>& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            var_.value_.object_->set(std::move(name),value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    void set(const std::basic_string<Char>& name, basic_json<Char, Alloc>&& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            var_.value_.object_->set(name,std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    void set(std::basic_string<Char>&& name, basic_json<Char, Alloc>&& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            var_.value_.object_->set(std::move(name),std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    object_iterator set(object_iterator hint, const std::basic_string<Char>& name, const basic_json<Char, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            return var_.value_.object_->set(hint, name, value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object", name);
            }
        }
    }

    object_iterator set(object_iterator hint, std::basic_string<Char>&& name, const basic_json<Char, Alloc>& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            return var_.value_.object_->set(hint, std::move(name),value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    object_iterator set(object_iterator hint, const std::basic_string<Char>& name, basic_json<Char, Alloc>&& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            return var_.value_.object_->set(hint, name,std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    object_iterator set(object_iterator hint, std::basic_string<Char>&& name, basic_json<Char, Alloc>&& value){
        switch (var_.type_){
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
        case value_types::object_t:
            return var_.value_.object_->set(hint, std::move(name),std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION_1(std::domain_error,"Attempting to set %s on a value that is not an object",name);
            }
        }
    }

    void add(const basic_json<Char, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_->push_back(value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::domain_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    void add(basic_json<Char, Alloc>&& value){
        switch (var_.type_){
        case value_types::array_t:
            var_.value_.array_->push_back(std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::domain_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    void add(size_t index, const basic_json<Char, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_->add(index, value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::domain_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    void add(size_t index, basic_json<Char, Alloc>&& value){
        switch (var_.type_){
        case value_types::array_t:
            var_.value_.array_->add(index, std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::domain_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    void add(const_array_iterator pos, const basic_json<Char, Alloc>& value)
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            var_.value_.array_->add(pos, value);
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::domain_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    void add(const_array_iterator pos, basic_json<Char, Alloc>&& value){
        switch (var_.type_){
        case value_types::array_t:
            var_.value_.array_->add(pos, std::move(value));
            break;
        default:
            {
                JSONCONS_THROW_EXCEPTION(std::domain_error,"Attempting to insert into a value that is not an array");
            }
        }
    }

    value_types::value_types_t type() const
    {
        return var_.type_;
    }

    void to_stream(basic_json_output_handler<Char>& handler) const;

    void swap(basic_json<Char,Alloc>& b)
    {
        var_.swap(b.var_);
    }

    template <class T>
    std::vector<T> as_vector() const
    {
        std::vector<T> v(size());
        for (size_t i = 0; i < v.size(); ++i)
        {
            v[i] = json_type_traits<Char,Alloc,T>::as(at(i));
        }
        return v;
    }

    friend void swap(basic_json<Char,Alloc>& a, basic_json<Char,Alloc>& b)
    {
        a.swap(b);
    }

    void assign_any(const typename basic_json<Char,Alloc>::any& rhs)
    {
        var_.assign(rhs);
    }

    void assign_string(const std::basic_string<Char>& rhs)
    {
        var_.assign(rhs);
    }

    void assign_string(const Char* rhs, size_t length)
    {
        var_.assign_string(rhs,length);
    }

    void assign_bool(bool rhs)
    {
        var_.assign(rhs);
    }

    void assign_object(const json_object<basic_json<Char,Alloc>,Alloc>& rhs)
    {
        var_.assign(rhs);
    }

    void assign_array(const json_array<basic_json<Char,Alloc>,Alloc>& rhs)
    {
        var_.assign(rhs);
    }

    void assign_null()
    {
        var_.assign(null_type());
    }

    template <typename T>
    const T& any_cast() const
    {
        if (var_.type_ != value_types::any_t)
        {
            JSONCONS_THROW_EXCEPTION(std::exception,"Bad any cast");
        }
        return var_.value_.any_value_->template cast<T>();
    }
    template <typename T>
    T& any_cast() 
    {
        if (var_.type_ != value_types::any_t)
        {
            JSONCONS_THROW_EXCEPTION(std::exception,"Bad any cast");
        }
        return var_.value_.any_value_->template cast<T>();
    }

    void assign_double(double rhs)
    {
        var_.assign(rhs);
    }
    void assign_longlong(long long rhs)
    {
        var_.assign(rhs);
    }
    void assign_ulonglong(unsigned long long rhs)
    {
        var_.assign(rhs);
    }

    static basic_json make_2d_array(size_t m, size_t n);

    template <typename T>
    static basic_json make_2d_array(size_t m, size_t n, T val);

    static basic_json make_3d_array(size_t m, size_t n, size_t k);

    template <typename T>
    static basic_json make_3d_array(size_t m, size_t n, size_t k, T val);

    template<int size>
    static typename std::enable_if<size==1,basic_json>::type make_multi_array()
    {
        return build_array<Char,Alloc,size>()();
    }
    template<size_t size>
    static typename std::enable_if<size==1,basic_json>::type make_multi_array(size_t n)
    {
        return build_array<Char,Alloc,size>()(n);
    }
    template<size_t size,typename T>
    static typename std::enable_if<size==1,basic_json>::type make_multi_array(size_t n, T val)
    {
        return build_array<Char,Alloc,size>()(n, val);
    }
    template<size_t size>
    static typename std::enable_if<size==2,basic_json>::type make_multi_array(size_t m, size_t n)
    {
        return build_array<Char,Alloc,size>()(m, n);
    }
    template<size_t size,typename T>
    static typename std::enable_if<size==2,basic_json>::type make_multi_array(size_t m, size_t n, T val)
    {
        return build_array<Char,Alloc,size>()(m, n, val);
    }
    template<size_t size>
    static typename std::enable_if<size==3,basic_json>::type make_multi_array(size_t m, size_t n, size_t k)
    {
        return build_array<Char,Alloc,size>()(m, n, k);
    }
    template<size_t size,typename T>
    static typename std::enable_if<size==3,basic_json>::type make_multi_array(size_t m, size_t n, size_t k, T val)
    {
        return build_array<Char,Alloc,size>()(m, n, k, val);
    }

    object_range members()
    {
        static object empty;
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return object_range(empty);
        case value_types::object_t:
            return object_range(object_value());
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not an object");
        }
    }

    const_object_range members() const
    {
        static const object empty;
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return const_object_range(empty);
        case value_types::object_t:
            return const_object_range(object_value());
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not an object");
        }
    }

    array_range elements()
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return array_range(array_value());
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not an array");
        }
    }

    const_array_range elements() const
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return const_array_range(array_value());
        default:
            JSONCONS_THROW_EXCEPTION(std::domain_error,"Not an array");
        }
    }

    array& array_value() 
    {
		switch (var_.type_)
		{
        case value_types::array_t:
            return *(var_.value_.array_);
        default:
            JSONCONS_THROW_EXCEPTION(std::exception,"Bad array cast");
            break;
        }
    }

    const array& array_value() const
    {
        switch (var_.type_)
        {
        case value_types::array_t:
            return *(var_.value_.array_);
        default:
            JSONCONS_THROW_EXCEPTION(std::exception,"Bad array cast");
            break;
        }
    }

    object& object_value()
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            var_.type_ = value_types::object_t;
            var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
            return *(var_.value_.object_);
        case value_types::object_t:
            return *(var_.value_.object_);
        default:
            JSONCONS_THROW_EXCEPTION(std::exception,"Bad object cast");
            break;
        }
    }

    const object& object_value() const
    {
        switch (var_.type_)
        {
        case value_types::empty_object_t:
            return cobject().object_value();
        case value_types::object_t:
            return *(var_.value_.object_);
        default:
            JSONCONS_THROW_EXCEPTION(std::exception,"Bad object cast");
            break;
        }
    }

private:
    const basic_json<Char,Alloc>& cobject() const
    {
        static const basic_json<Char, Alloc> c = basic_json<Char, Alloc>(value_types::object_t,0);
        return c;
    }

    template<typename Char2, typename Allocator2, size_t size>
    class build_array
    {};
    template<typename Char2, typename Allocator2>
    class build_array<Char2,Allocator2,1>
    {
    public:
        basic_json<Char2,Allocator2> operator() ()
        {
            return basic_json<Char2,Allocator2>::make_array();
        }
        basic_json<Char2,Allocator2> operator() (size_t n)
        {
            return basic_json<Char2,Allocator2>::make_array(n);
        }
        template <typename T>
        basic_json<Char2,Allocator2> operator() (size_t n, T val)
        {
            return basic_json<Char2,Allocator2>::make_array(n, val);
        }
    };
    template<typename Char2, typename Allocator2>
    class build_array<Char2,Allocator2,2>
    {
    public:
        basic_json<Char2,Allocator2> operator() (size_t m, size_t n)
        {
            return basic_json<Char2,Allocator2>::make_2d_array(m, n);
        }
        template <typename T>
        basic_json<Char2,Allocator2> operator() (size_t m, size_t n, T val)
        {
            return basic_json<Char2,Allocator2>::make_2d_array(m, n, val);
        }
    };
    template<typename Char2, typename Allocator2>
    class build_array<Char2,Allocator2,3>
    {
    public:
        basic_json<Char2,Allocator2> operator() (size_t m, size_t n, size_t k)
        {
            return basic_json<Char2,Allocator2>::make_3d_array (m, n, k);
        }
        template <typename T>
        basic_json<Char2,Allocator2> operator() (size_t m, size_t n, size_t k, T val)
        {
            return basic_json<Char2,Allocator2>::make_3d_array (m, n, k, val);
        }
    };

    friend std::basic_ostream<Char>& operator<<(std::basic_ostream<Char>& os, const basic_json<Char, Alloc>& o)
    {
        o.to_stream(os);
        return os;
    }

    friend std::basic_istream<Char>& operator<<(std::basic_istream<Char>& is, basic_json<Char, Alloc>& o)
    {
        basic_json_deserializer<Char, Alloc> handler;
        basic_json_reader<Char> reader(is, handler);
        reader.read_next();
        reader.check_done();
        if (!handler.is_valid())
        {
            JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json stream");
        }
        o = handler.get_result();
        return is;
    }

private:
	variant var_;
};

template <typename Char, typename Alloc>
void swap(typename basic_json<Char,Alloc>::member_type& a, typename basic_json<Char,Alloc>::member_type& b)
{
    a.swap(b);
}

template<typename Char, typename Alloc>
bool basic_json<Char, Alloc>::operator!=(const basic_json<Char, Alloc>& rhs) const
{
    return !(*this == rhs);
}

template<typename Char, typename Alloc>
bool basic_json<Char, Alloc>::operator==(const basic_json<Char, Alloc>& rhs) const
{
    return var_ == rhs.var_;
}

template<typename Char, typename Alloc>
const basic_json<Char, Alloc>& basic_json<Char, Alloc>::get(const std::basic_string<Char>& name) const
{
    static const basic_json<Char, Alloc> a_null = null_type();

    switch (var_.type_)
    {
    case value_types::empty_object_t:
        return a_null;
    case value_types::object_t:
        {
            const_object_iterator it = var_.value_.object_->find(name);
            return it != end_members() ? it->value() : a_null;
        }
    default:
        {
            JSONCONS_THROW_EXCEPTION_1(std::exception,"Attempting to get %s from a value that is not an object", name);
        }
    }
}

template<typename Char, typename Alloc>
template<typename T>
basic_json<Char, Alloc> basic_json<Char, Alloc>::get(const std::basic_string<Char>& name, const T& default_val) const
{
    switch (var_.type_)
    {
    case value_types::empty_object_t:
        {
            return basic_json<Char,Alloc>(default_val);
        }
    case value_types::object_t:
        {
            const_object_iterator it = var_.value_.object_->find(name);
            if (it != end_members())
            {
                return it->value();
            }
            else
            {
                return basic_json<Char,Alloc>(default_val);
            }
        }
    default:
        {
            JSONCONS_THROW_EXCEPTION_1(std::exception,"Attempting to get %s from a value that is not an object", name);
        }
    }
}

template<typename Char, typename Alloc>
void basic_json<Char, Alloc>::clear()
{
    switch (var_.type_)
    {
    case value_types::array_t:
        var_.value_.array_->clear();
        break;
    case value_types::object_t:
        var_.value_.object_->clear();
        break;
    default:
        break;
    }
}

template<typename Char, typename Alloc>
std::basic_string<Char> basic_json<Char, Alloc>::to_string() const
{
    std::basic_ostringstream<Char> os;
    {
        basic_json_serializer<Char> serializer(os);
        to_stream(serializer);
    }
    return os.str();
}

template<typename Char, typename Alloc>
std::basic_string<Char> basic_json<Char, Alloc>::to_string(const basic_output_format<Char>& format) const
{
    std::basic_ostringstream<Char> os;
    {
        basic_json_serializer<Char> serializer(os, format);
        to_stream(serializer);
    }
    return os.str();
}

template<typename Char, typename Alloc>
void basic_json<Char, Alloc>::to_stream(basic_json_output_handler<Char>& handler) const
{
    switch (var_.type_)
    {
    case value_types::small_string_t:
        handler.value(var_.value_.small_string_value_,var_.small_string_length_);
        break;
    case value_types::string_t:
        handler.value(var_.value_.string_value_->c_str(),var_.value_.string_value_->length());
        break;
    case value_types::double_t:
        handler.value(var_.value_.float_value_);
        break;
    case value_types::integer_t:
        handler.value(var_.value_.integer_value_);
        break;
    case value_types::uinteger_t:
        handler.value(var_.value_.uinteger_value_);
        break;
    case value_types::bool_t:
        handler.value(var_.value_.bool_value_);
        break;
    case value_types::null_t:
        handler.value(null_type());
        break;
    case value_types::empty_object_t:
        handler.begin_object();
        handler.end_object();
        break;
    case value_types::object_t:
        {
            handler.begin_object();
            json_object<basic_json<Char,Alloc>,Alloc> *o = var_.value_.object_;
            for (const_object_iterator it = o->begin(); it != o->end(); ++it)
            {
                handler.name((it->name()).c_str(),it->name().length());
                it->value().to_stream(handler);
            }
            handler.end_object();
        }
        break;
    case value_types::array_t:
        {
            handler.begin_array();
            json_array<basic_json<Char,Alloc>,Alloc> *o = var_.value_.array_;
            for (const_array_iterator it = o->begin(); it != o->end(); ++it)
            {
                it->to_stream(handler);
            }
            handler.end_array();
        }
        break;
    case value_types::any_t:
        var_.value_.any_value_->to_stream(handler);
        break;
    default:
        break;
    }
}

template<typename Char, typename Alloc>
void basic_json<Char, Alloc>::to_stream(std::basic_ostream<Char>& os) const
{
    basic_json_serializer<Char> serializer(os);
    to_stream(serializer);
}

template<typename Char, typename Alloc>
void basic_json<Char, Alloc>::to_stream(std::basic_ostream<Char>& os, const basic_output_format<Char>& format) const
{
    basic_json_serializer<Char> serializer(os, format);
    to_stream(serializer);
}

template<typename Char, typename Alloc>
void basic_json<Char, Alloc>::to_stream(std::basic_ostream<Char>& os, const basic_output_format<Char>& format, bool indenting) const
{
    basic_json_serializer<Char> serializer(os, format, indenting);
    to_stream(serializer);
}


// Deprecated static data members
template<typename Char, typename Alloc>
const basic_json<Char, Alloc> basic_json<Char, Alloc>::an_object = basic_json<Char, Alloc>(value_types::object_t,0);
template<typename Char, typename Alloc>
const basic_json<Char, Alloc> basic_json<Char, Alloc>::an_array = basic_json<Char, Alloc>(value_types::array_t,0);        
template<typename Char, typename Alloc>
const basic_json<Char, Alloc> basic_json<Char, Alloc>::null = basic_json<Char, Alloc>(jsoncons::null_type());

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::make_2d_array(size_t m, size_t n)
{
    basic_json<Char, Alloc> a(basic_json<Char, Alloc>(json_array<basic_json<Char,Alloc>,Alloc>()));
    a.resize(m);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = basic_json<Char, Alloc>::make_array(n);
    }
    return a;
}

template<typename Char, typename Alloc>
template<typename T>
basic_json<Char, Alloc> basic_json<Char, Alloc>::make_2d_array(size_t m, size_t n, T val)
{
    basic_json<Char, Alloc> v;
    v = val;
    basic_json<Char, Alloc> a = make_array(m);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = basic_json<Char, Alloc>::make_array(n, v);
    }
    return a;
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::make_3d_array(size_t m, size_t n, size_t k)
{
    basic_json<Char, Alloc> a(basic_json<Char, Alloc>(json_array<basic_json<Char,Alloc>,Alloc>()));
    a.resize(m);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = basic_json<Char, Alloc>::make_2d_array(n, k);
    }
    return a;
}

template<typename Char, typename Alloc>
template<typename T>
basic_json<Char, Alloc> basic_json<Char, Alloc>::make_3d_array(size_t m, size_t n, size_t k, T val)
{
    basic_json<Char, Alloc> v;
    v = val;
    basic_json<Char, Alloc> a = make_array(m);
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] = basic_json<Char, Alloc>::make_2d_array(n, k, v);
    }
    return a;
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::parse_stream(std::basic_istream<Char>& is)
{
    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    basic_json_reader<Char> reader(is, handler);
    reader.read_next();
    reader.check_done();
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json stream");
    }
    return handler.get_result();
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::parse_stream(std::basic_istream<Char>& is, 
                                                              basic_parse_error_handler<Char>& err_handler)
{
    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    basic_json_reader<Char> reader(is, handler, err_handler);
    reader.read_next();
    reader.check_done();
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json stream");
    }
    return handler.get_result();
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::parse(const std::basic_string<Char>& s)
{
    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    basic_json_parser<Char> parser(handler);
    parser.begin_parse();
    parser.parse(s.c_str(),0,s.length());
    parser.end_parse();
    parser.check_done(s.c_str(),parser.index(),s.length());
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json string");
    }
    return handler.get_result();
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::parse(const std::basic_string<Char>& s, 
                                                       basic_parse_error_handler<Char>& err_handler)
{
    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    basic_json_parser<Char> parser(handler,err_handler);
    parser.begin_parse();
    parser.parse(s.c_str(),0,s.length());
    parser.end_parse();
    parser.check_done(s.c_str(),parser.index(),s.length());
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json string");
    }
    return handler.get_result();
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::parse_string(const std::basic_string<Char>& s)
{
    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    basic_json_parser<Char> parser(handler);
    parser.begin_parse();
    parser.parse(s.c_str(),0,s.length());
    parser.end_parse();
    parser.check_done(s.c_str(),parser.index(),s.length());
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json string");
    }
    return handler.get_result();
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::parse_string(const std::basic_string<Char>& s, 
                                                              basic_parse_error_handler<Char>& err_handler)
{
    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    basic_json_parser<Char> parser(handler,err_handler);
    parser.begin_parse();
    parser.parse(s.c_str(),0,s.length());
    parser.end_parse();
    parser.check_done(s.c_str(),parser.index(),s.length());
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json string");
    }
    return handler.get_result();
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::parse_file(const std::string& filename)
{
    FILE* fp;


#if defined(_MSC_VER)
    errno_t err = fopen_s(&fp, filename.c_str(), "rb");
    if (err != 0) 
    {
        JSONCONS_THROW_EXCEPTION_1(std::exception,"Cannot open file %s", filename);
    }
#else
    fp = std::fopen(filename.c_str(), "rb");
    if (fp == nullptr)
    {
        JSONCONS_THROW_EXCEPTION_1(std::exception,"Cannot open file %s", filename);
    }
#endif
    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    try
    {
        // obtain file size:
        std::fseek (fp , 0 , SEEK_END);
        long size = std::ftell (fp);
        std::rewind(fp);

        if (size > 0)
        {
            std::vector<Char> buffer(size);

            // copy the file into the buffer:
            size_t result = std::fread (buffer.data(),1,size,fp);
            if (result != static_cast<unsigned long long>(size))
            {
                JSONCONS_THROW_EXCEPTION_1(std::exception,"Error reading file %s", filename);
            }

            basic_json_parser<Char> parser(handler);
            parser.begin_parse();
            parser.parse(buffer.data(),0,buffer.size());
            parser.end_parse();
            parser.check_done(buffer.data(),parser.index(),buffer.size());
        }

        std::fclose (fp);
    }
    catch (...)
    {
        std::fclose (fp);
        throw;
    }
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json file");
    }
    return handler.get_result();
}

template<typename Char, typename Alloc>
basic_json<Char, Alloc> basic_json<Char, Alloc>::parse_file(const std::string& filename, 
                                                            basic_parse_error_handler<Char>& err_handler)
{
    FILE* fp;

#if defined(_MSC_VER)
    errno_t err = fopen_s(&fp, filename.c_str(), "rb");
    if (err != 0) 
    {
        JSONCONS_THROW_EXCEPTION_1(std::exception,"Cannot open file %s", filename);
    }
#else
    fp = std::fopen(filename.c_str(), "rb");
    if (fp == nullptr)
    {
        JSONCONS_THROW_EXCEPTION_1(std::exception,"Cannot open file %s", filename);
    }
#endif

    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    try
    {
        // obtain file size:
        std::fseek (fp , 0 , SEEK_END);
        long size = std::ftell (fp);
        std::rewind(fp);

        if (size > 0)
        {
            std::vector<Char> buffer(size);

            // copy the file into the buffer:
            size_t result = std::fread (buffer.data(),1,size,fp);
            if (result != static_cast<unsigned long long>(size))
            {
                JSONCONS_THROW_EXCEPTION_1(std::exception,"Error reading file %s", filename);
            }

            basic_json_parser<Char> parser(handler,err_handler);
            parser.begin_parse();
            parser.parse(buffer.data(),0,buffer.size());
            parser.end_parse();
            parser.check_done(buffer.data(),parser.index(),buffer.size());
        }

        std::fclose (fp);
    }
    catch (...)
    {
        std::fclose (fp);
        throw;
    }
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json file");
    }
    return handler.get_result();
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::object_iterator basic_json<Char, Alloc>::begin_members()
{
    switch (var_.type_)
    {
    case value_types::empty_object_t:
        var_.type_ = value_types::object_t;
        var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
    case value_types::object_t:
        return var_.value_.object_->begin();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an object");
    }
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::const_object_iterator basic_json<Char, Alloc>::begin_members() const
{
    switch (var_.type_)
    {
    case value_types::empty_object_t:
        return cobject().begin_members();
    case value_types::object_t:
        return var_.value_.object_->begin();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an object");
    }
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::object_iterator basic_json<Char, Alloc>::end_members()
{
    switch (var_.type_)
    {
    case value_types::empty_object_t:
        var_.type_ = value_types::object_t;
        var_.value_.object_ = new json_object<basic_json<Char,Alloc>,Alloc>();
    case value_types::object_t:
        return var_.value_.object_->end();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an object");
    }
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::const_object_iterator basic_json<Char, Alloc>::end_members() const
{
    switch (var_.type_)
    {
    case value_types::empty_object_t:
        return cobject().end_members();
    case value_types::object_t:
        return var_.value_.object_->end();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an object");
    }
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::array_iterator basic_json<Char, Alloc>::begin_elements()
{
    switch (var_.type_)
    {
    case value_types::array_t:
        return var_.value_.array_->begin();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an array");
    }
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::const_array_iterator basic_json<Char, Alloc>::begin_elements() const
{
    switch (var_.type_)
    {
    case value_types::array_t:
        return var_.value_.array_->begin();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an array");
    }
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::array_iterator basic_json<Char, Alloc>::end_elements()
{
    switch (var_.type_)
    {
    case value_types::array_t:
        return var_.value_.array_->end();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an array");
    }
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::const_array_iterator basic_json<Char, Alloc>::end_elements() const
{
    switch (var_.type_)
    {
    case value_types::array_t:
        return var_.value_.array_->end();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an array");
    }
}

template<typename Char, typename Alloc>
bool basic_json<Char, Alloc>::has_member(const std::basic_string<Char>& name) const
{
    switch (var_.type_)
    {
    case value_types::object_t:
        {
            const_object_iterator it = var_.value_.object_->find(name);
            return it != end_members();
        }
        break;
    default:
        return false;
    }
}

template<typename Char, typename Alloc>
int basic_json<Char, Alloc>::as_int() const
{
    switch (var_.type_)
    {
    case value_types::double_t:
        return static_cast<int>(var_.value_.float_value_);
    case value_types::integer_t:
        return static_cast<int>(var_.value_.integer_value_);
    case value_types::uinteger_t:
        return static_cast<int>(var_.value_.uinteger_value_);
    case value_types::bool_t:
        return var_.value_.bool_value_ ? 1 : 0;
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not a int");
    }
}

template<typename Char, typename Alloc>
unsigned int basic_json<Char, Alloc>::as_uint() const
{
    switch (var_.type_)
    {
    case value_types::double_t:
        return static_cast<unsigned int>(var_.value_.float_value_);
    case value_types::integer_t:
        return static_cast<unsigned int>(var_.value_.integer_value_);
    case value_types::uinteger_t:
        return static_cast<unsigned int>(var_.value_.uinteger_value_);
    case value_types::bool_t:
        return var_.value_.bool_value_ ? 1 : 0;
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not a unsigned int");
    }
}

template<typename Char, typename Alloc>
long basic_json<Char, Alloc>::as_long() const
{
    switch (var_.type_)
    {
    case value_types::double_t:
        return static_cast<long>(var_.value_.float_value_);
    case value_types::integer_t:
        return static_cast<long>(var_.value_.integer_value_);
    case value_types::uinteger_t:
        return static_cast<long>(var_.value_.uinteger_value_);
    case value_types::bool_t:
        return var_.value_.bool_value_ ? 1 : 0;
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not a long");
    }
}

template<typename Char, typename Alloc>
unsigned long basic_json<Char, Alloc>::as_ulong() const
{
    switch (var_.type_)
    {
    case value_types::double_t:
        return static_cast<unsigned long>(var_.value_.float_value_);
    case value_types::integer_t:
        return static_cast<unsigned long>(var_.value_.integer_value_);
    case value_types::uinteger_t:
        return static_cast<unsigned long>(var_.value_.uinteger_value_);
    case value_types::bool_t:
        return var_.value_.bool_value_ ? 1 : 0;
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an unsigned long");
    }
}

template<typename Char, typename Alloc>
typename basic_json<Char, Alloc>::any& basic_json<Char, Alloc>::any_value()
{
    switch (var_.type_)
    {
    case value_types::any_t:
        {
			return *var_.value_.any_value_;
        }
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an any value");
    }
}

template<typename Char, typename Alloc>
const typename basic_json<Char, Alloc>::any& basic_json<Char, Alloc>::any_value() const
{
    switch (var_.type_)
    {
    case value_types::any_t:
        {
			return *var_.value_.any_value_;
        }
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not an any value");
    }
}

template<typename Char, typename Alloc>
const Char* basic_json<Char, Alloc>::as_cstring() const
{
    switch (var_.type_)
    {
    case value_types::small_string_t:
        return var_.value_.small_string_value_;
    case value_types::string_t:
        return var_.value_.string_value_->c_str();
    default:
        JSONCONS_THROW_EXCEPTION(std::exception,"Not a string");
    }
}

template <typename Char, typename Alloc>
std::basic_istream<Char>& operator>>(std::basic_istream<Char>& is, basic_json<Char, Alloc>& o)
{
    basic_json_deserializer<basic_json<Char, Alloc>> handler;
    basic_json_reader<Char> reader(is, handler);
    reader.read_next();
    reader.check_done();
    if (!handler.is_valid())
    {
        JSONCONS_THROW_EXCEPTION(std::exception,"Failed to parse json stream");
    }
    o = handler.get_result();
    return is;
}

template<typename Char, typename Alloc>
class json_printable
{
public:
    json_printable(const basic_json<Char, Alloc>& o,
                   bool is_pretty_print)
       : o_(&o), is_pretty_print_(is_pretty_print)
    {
    }

    json_printable(const basic_json<Char, Alloc>& o,
                   bool is_pretty_print,
                   const basic_output_format<Char>& format)
       : o_(&o), is_pretty_print_(is_pretty_print), format_(format)
    {
        ;
    }

    void to_stream(std::basic_ostream<Char>& os) const
    {
        o_->to_stream(os, format_, is_pretty_print_);
    }

    friend std::basic_ostream<Char>& operator<<(std::basic_ostream<Char>& os, const json_printable<Char, Alloc>& o)
    {
        o.to_stream(os);
        return os;
    }

    const basic_json<Char, Alloc> *o_;
    bool is_pretty_print_;
    basic_output_format<Char> format_;
private:
    json_printable();
};

template<typename Char, class Alloc>
json_printable<Char, Alloc> print(const basic_json<Char, Alloc>& val)
{
    return json_printable<Char, Alloc>(val,false);
}

template<typename Char, class Alloc>
json_printable<Char, Alloc> print(const basic_json<Char, Alloc>& val,
                                  const basic_output_format<Char>& format)
{
    return json_printable<Char, Alloc>(val, false, format);
}

template<typename Char, class Alloc>
json_printable<Char, Alloc> pretty_print(const basic_json<Char, Alloc>& val)
{
    return json_printable<Char, Alloc>(val,true);
}

template<typename Char, class Alloc>
json_printable<Char, Alloc> pretty_print(const basic_json<Char, Alloc>& val,
                                         const basic_output_format<Char>& format)
{
    return json_printable<Char, Alloc>(val, true, format);
}

typedef basic_json<char,std::allocator<void>> json;
typedef basic_json<wchar_t,std::allocator<void>> wjson;

typedef basic_json_deserializer<json> json_deserializer;
typedef basic_json_deserializer<wjson> wjson_deserializer;

}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
