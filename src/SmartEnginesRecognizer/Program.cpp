#include "smartengines/passport_engine.h"

#include "jsoncons/json.hpp"
using jsoncons::json;
using jsoncons::pretty_print;

#include "tinydir/tinydir.h"

#include <direct.h>

#define RECOGNIZER_ID "smartengines"

double diffclock(clock_t end, clock_t start)
{
	double ticks = end - start;
	return ticks / (CLOCKS_PER_SEC / 1000);
}

json ValueToJson(PassportStringField field)
{
	json result;
	result["value"] = field.value;
	result["confidence"] = field.is_accepted ? "1" : "0";
	return result;
}

json ValueToJson(PassportGenderField field)
{
	json result;
	result["value"] = field.ToString();
	result["confidence"] = field.is_accepted ? "1" : "0";
	return result;
}

json ValueToJson(PassportDateField field)
{
	json result;
	result["value"] = field.ToString();
	result["confidence"] = field.is_accepted ? "1" : "0";
	return result;
}

json ValueToJson(PassportCodeField field)
{
	json result;
	result["value"] = field.ToString();
	result["confidence"] = field.is_accepted ? "1" : "0";
	return result;
}

struct ResultReporter : PassportResultReporterInterface
{
	PassportEngine *engine;

	clock_t start;
	clock_t end;
	double time;

	std::string image_path;
	bool snapshot_rejected = false;
	json matches = json::array();
	json data;

	explicit ResultReporter(PassportEngine *passportEngine, std::string path)
	{
		engine = passportEngine;
		image_path = path;
	}

	void ProcessImage()
	{
		start = clock();

		engine->InitializeSession(*this);
		engine->ProcessImageFile(image_path);
		engine->TerminateSession();

		end = clock();
		time = diffclock(end, start);
	}

	std::string GetResult() const
	{
		json result;
		result["image_path"] = image_path;
		result["snapshot_rejected"] = snapshot_rejected;
		result["matches"] = matches;
		result["data"] = data;
		result["time"] = time;
		
		return result.as_string();
	}

	virtual void SnapshotRejected() override
	{
		snapshot_rejected = true;
	}

	virtual void DocumentMatched(const PassportMatchResult &result, PassportImageRequest &request) override
	{
		json match;
		match["score"] = result.score;
		match["type"] = result.type;
		matches.add(match);
	}

	virtual void SnapshotProcessed(const PassportRecognitionResult &result, bool may_finish, bool is_break) override
	{
		data["enough_data"]    = may_finish;
		data["series"]         = ValueToJson(result.series);
		data["number"]         = ValueToJson(result.number);
		data["surname"]        = ValueToJson(result.surname);
		data["name"]           = ValueToJson(result.name);
		data["patronymic"]     = ValueToJson(result.patronymic);
		data["gender"]         = ValueToJson(result.gender);
		data["birthdate"]      = ValueToJson(result.birthdate);
		data["birthplace"]     = ValueToJson(result.birthplace);
		data["authority"]      = ValueToJson(result.authority);
		data["issue_date"]     = ValueToJson(result.issue_date);
		data["authority_code"] = ValueToJson(result.authority_code);
		data["mrz_line1"]      = ValueToJson(result.mrz_line1);
		data["mrz_line2"]      = ValueToJson(result.mrz_line2);
	}
};

void ProcessData(std::string data_path, std::string result_path, PassportEngine *engine)
{
	tinydir_dir data_dir;
	if (tinydir_open(&data_dir, data_path.c_str()) == -1)
	{
		std::cout << std::endl;
		std::cout << "Failed to open data directory" << std::endl;
	}

	tinydir_dir result_dir;
	if (tinydir_open(&result_dir, result_path.c_str()) == -1)
	{
		std::cout << std::endl;
		std::cout << "Failed to open result directory" << std::endl;
	}

	while (data_dir.has_next)
	{
		tinydir_file data_pack_dir_file;
		if (tinydir_readfile(&data_dir, &data_pack_dir_file) != -1 && data_pack_dir_file.is_dir)
		{
			std::string result_dir_path = result_path + RECOGNIZER_ID + "/" + data_pack_dir_file.name;
			_mkdir(result_dir_path.c_str());

			tinydir_dir data_pack_dir;
			if (tinydir_open(&data_pack_dir, data_pack_dir_file.path) != -1)
			{
				while (data_pack_dir.has_next)
				{
					tinydir_file data_pack_image_file;

					try
					{
						if (tinydir_readfile(&data_pack_dir, &data_pack_image_file) != -1 && data_pack_image_file.is_reg)
						{
							std::cout << data_pack_image_file.path << std::endl;

							auto reporter = new ResultReporter(engine, data_pack_image_file.path);
							reporter->ProcessImage();
							auto result = reporter->GetResult();

							auto result_file_path = result_path + RECOGNIZER_ID + "/" + data_pack_dir_file.name + "/" + data_pack_image_file.name + ".json";

							std::ofstream result_file;
							result_file.open(result_file_path);
							result_file << result;
							result_file.close();
						}
					}
					catch (...) {
						std::cout << std::endl;
						std::cout << "File exception: " << data_pack_image_file.path << std::endl;
					}

					tinydir_next(&data_pack_dir);
				}
			}
		}

		tinydir_next(&data_dir);
	}
}

int main(int argc, char **argv) {
	std::string data_path = "../../data/";
	std::string result_path = "../../result/";
	std::string config_path = "data/passport_anywhere.json";

	if (argc == 4)
	{
		data_path = argv[1];
		result_path = argv[2];
		config_path = argv[3];
	}

	std::cout << std::endl;
	std::cout << "Data path:   " << data_path   << std::endl;
	std::cout << "Result path: " << result_path << std::endl;
	std::cout << "Config path: " << config_path << std::endl;
	std::cout << std::endl;

	try {
		PassportEngine engine;
		engine.Configure(config_path);

		ProcessData(data_path, result_path, &engine);
	}
	catch (const PassportException &e) {
		std::cout << std::endl;
		std::cout << "Exception: " << e.what() << std::endl;
	}

	return 0;
}