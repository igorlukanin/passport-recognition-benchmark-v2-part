var compression = require('compression'),
    csv         = require('ya-csv'),
    ejs         = require('ejs'),
    express     = require('express'),
    fs          = require('fs'),
    lev         = require('levenshtein'),

    dataPath    = process.argv[2] || 'data/',
    resultPath  = process.argv[3] || 'result/',
    featurePath = process.argv[4],

    goodCsvPath = 'data/good.csv',
    port        = 3000;


var launchServer = function(data) {
    express()
        .use(express.static('public'))
        .use(express.static(dataPath))
        .use(express.static(resultPath))
        .use(compression())

        .get('/', function (req, res) {
            res.render('index', data);
        })

        .set('view engine', 'ejs')

        .listen(port, function () {
            console.log("Ready at port " + port);
        });
};

var unit_k = function(value) {
    return value.indexOf('Ki') > 0 ? parseFloat(value.substring(0, value.indexOf('Ki'))) * 1024 :
           value.indexOf('Mi') > 0 ? parseFloat(value.substring(0, value.indexOf('Mi'))) * 1024 * 1024 : undefined;
};

var accuracy = function(correct, sample) {
    correct = correct.toUpperCase().replace(/ /g, '');
    sample = sample.toUpperCase().replace(/ /g, '');

    return correct == sample ? 0 :
           sample == '' ? -1 :
           new lev(correct, sample).distance;

};

var seMatch = function(entry) {
    entry.se.accuracy = {
        series:     0,
        number:     0,
        surname:    0,
        name:       0,
        patronymic: 0,
        date:       0,
        authority:  0,
        z1:         0,
        z2:         0,
        z3:         0
    };

    entry.se.accuracy.series     = accuracy(entry.series,        entry.se.data && entry.se.data.series     ? entry.se.data.series.value : '');
    entry.se.accuracy.number     = accuracy(entry.number,        entry.se.data && entry.se.data.number     ? entry.se.data.number.value : '');
    entry.se.accuracy.surname    = accuracy(entry.surname,       entry.se.data && entry.se.data.surname    ? entry.se.data.surname.value : '');
    entry.se.accuracy.name       = accuracy(entry.name,          entry.se.data && entry.se.data.name       ? entry.se.data.name.value : '');
    entry.se.accuracy.patronymic = accuracy(entry.patronymic,    entry.se.data && entry.se.data.patronymic ? entry.se.data.patronymic.value : '');
    entry.se.accuracy.date       = accuracy(entry.deliverydate,  entry.se.data && entry.se.data.issue_date ? entry.se.data.issue_date.value : '');
    entry.se.accuracy.authority  = accuracy(entry.deliveryplace, entry.se.data && entry.se.data.authority  ? entry.se.data.authority.value : '');

    //if (entry.se.accuracy.authority > 0) {
    //    console.log(entry.id);
    //    //console.log(entry.series);
    //    //console.log(entry.se.data.series.value);
    //    //console.log(entry);
    //    //console.log(entry.se.accuracy);
    //}

    entry.se.accuracy.z1         = (entry.se.accuracy.series     == 0 ? 0 : 1) +
                                   (entry.se.accuracy.number     == 0 ? 0 : 1) +
                                   (entry.se.accuracy.surname    == 0 ? 0 : 1) +
                                   (entry.se.accuracy.name       == 0 ? 0 : 1) +
                                   (entry.se.accuracy.patronymic == 0 ? 0 : 1) +
                                   (entry.se.accuracy.date       == 0 ? 0 : 1) +
                                   (entry.se.accuracy.authority  == 0 ? 0 : 1);

    entry.se.accuracy.z2         = (entry.se.accuracy.series     == 0 ? 0 : 1) +
                                   (entry.se.accuracy.number     == 0 ? 0 : 1) +
                                   (entry.se.accuracy.surname    == 0 ? 0 : 1) +
                                   (entry.se.accuracy.name       == 0 ? 0 : 1) +
                                   (entry.se.accuracy.patronymic == 0 ? 0 : 1) +
                                   (entry.se.accuracy.date       == 0 ? 0 : 1);

    //entry.se.accuracy.z3         = entry.se.avg_confidence > 0.6 && entry.se.accuracy.z1 == 0 ? 1 : 0;
    //
    //if (entry.se.accuracy.z2 == 0 && entry.se.accuracy.z1 > 0) {
    //    console.log(entry.id);
    //    console.log(entry.deliveryplace);
    //    console.log(entry.se.data.authority.value);
    //}

    entry.se.accuracy.z3 = entry.se.time;

    return entry;
};

var pvMatch = function(entry) {
    entry.pv.accuracy = {
        z3: 0
    };

    entry.pv.accuracy.z3 = entry.pv.time;

    return entry;
};

var seRowsCounts = function(entry) {
    var confidence = 0;

    if (entry.se.data) {
        entry.se.data.rows_count = 0;

        for (var i in entry.se.data) {
            if (entry.se.data.hasOwnProperty(i) && entry.se.data[i].confidence == '0') {
                //delete entry.se.data[i];

                //continue;
            }

            if (entry.se.data.hasOwnProperty(i) && i != 'rows_count' && i != 'enough_data') {
                confidence += parseInt(entry.se.data[i].confidence);
                entry.se.data.rows_count += 1;
            }
        }
    }
    else {
        entry.se.data = {
            rows_count: 0
        };
    }

    entry.se.avg_confidence = entry.se.data.rows_count == 0 ? 0 : confidence / entry.se.data.rows_count;

    return entry;
};

var pvRowsCounts = function(entry) {
    var confidence = 0;

    if (entry.pv.data) {
        for (var i in entry.pv.data) {
            if (entry.pv.data.hasOwnProperty(i) && entry.pv.data[i].confidence == 0) {
                delete entry.pv.data[i];
                entry.pv.data.rows_count--;

                continue;
            }

            if (i != 'rows_count') {
                confidence += entry.pv.data[i].confidence;
            }
        }
    }

    //if (!entry.pv.failure) {
    //    console.log(entry.pv);
    //}

    if (entry.pv.failure) {
        entry.pv.data = {
            rows_count: 0
        };
    }

    entry.pv.avg_confidence = entry.pv.data.rows_count == 0 ? 0 : confidence / entry.pv.data.rows_count / 100;

    return entry;
};

var success = function(entry) {
    entry.se.success = entry.se.matches && entry.se.matches.length > 0;
    entry.pv.success = entry.pv.data && entry.pv.data.rows_count >= 6;

    return entry;
};

var loadData = function(callback) {
    var data = {
        entries: [],
        stat: {
            counts: {
                all: 0,
                se: 0,
                pv: 0
            }
        },
        charts: {
            se_rows_counts:         [ ['', ''] ],
            se_avg_confidence:      [ ['', ''] ],
            se_pages:               [ ['', ''] ],
            se_accuracy_series:     [ ['', ''] ],
            se_accuracy_number:     [ ['', ''] ],
            se_accuracy_surname:    [ ['', ''] ],
            se_accuracy_name:       [ ['', ''] ],
            se_accuracy_patronymic: [ ['', ''] ],
            se_accuracy_date:       [ ['', ''] ],
            se_accuracy_authority:  [ ['', ''] ],
            se_accuracy_z1:         [ ['', ''] ],
            se_accuracy_z2:         [ ['', ''] ],
            se_accuracy_z3:         [ ['', ''] ],

            pv_rows_counts:         [ ['', ''] ],
            pv_avg_confidence:      [ ['', ''] ],
            pv_accuracy_z3:         [ ['', ''] ]
        }
    };

    csv.createCsvFileReader(goodCsvPath, {
        'separator': ';',
        'quote': '',
        'escape': '',
        'comment': ''
    }).addListener('data', function(row) {
        var entry = {
            "id":            row[2],
            "series":        row[3],
            "number":        row[4],
            "surname":       row[5],
            "name":          row[6],
            "patronymic":    row[7],
            "deliverydate":  row[8].substr(8, 2) + '.' + row[8].substr(5, 2) + '.' + row[8].substr(0, 4),
            "deliveryplace": row[9]
        };

        var sePath = resultPath + 'smartengines/good/'   + entry.id + '.jpg.json';
        var pvPath = resultPath + 'passportvision/good/' + entry.id + '.jpg.json';

        entry.se = fs.existsSync(sePath) ? JSON.parse(fs.readFileSync(sePath)) : {};
        entry.pv = fs.existsSync(pvPath) ? JSON.parse(fs.readFileSync(pvPath)) : {};

        entry = seRowsCounts(entry);
        entry = pvRowsCounts(entry);

        entry = seMatch(entry);
        entry = pvMatch(entry);

        entry = success(entry);

        data.entries.push(entry);

        data.stat.counts.all += 1;
        data.stat.counts.se += entry.se.success ? 1 : 0;
        data.stat.counts.pv += entry.pv.success ? 1 : 0;

        data.charts.se_rows_counts.push([         '', entry.se.data.rows_count ]);
        data.charts.se_avg_confidence.push([      '', entry.se.avg_confidence ]);
        data.charts.se_pages.push([               '', !entry.se.matches ? 0 : entry.se.matches.length ]);
        data.charts.se_accuracy_series.push([     '', entry.se.accuracy.series ]);
        data.charts.se_accuracy_number.push([     '', entry.se.accuracy.number ]);
        data.charts.se_accuracy_surname.push([    '', entry.se.accuracy.surname ]);
        data.charts.se_accuracy_name.push([       '', entry.se.accuracy.name ]);
        data.charts.se_accuracy_patronymic.push([ '', entry.se.accuracy.patronymic ]);
        data.charts.se_accuracy_date.push([       '', entry.se.accuracy.date ]);
        data.charts.se_accuracy_authority.push([  '', entry.se.accuracy.authority ]);
        data.charts.se_accuracy_z1.push([         '', entry.se.accuracy.z1 ]);
        data.charts.se_accuracy_z2.push([         '', entry.se.accuracy.z2 ]);
        data.charts.se_accuracy_z3.push([         '', entry.se.accuracy.z3 ]);

        data.charts.pv_avg_confidence.push([      '', entry.pv.avg_confidence ]);
        data.charts.pv_rows_counts.push([         '', entry.pv.data.rows_count ]);
        data.charts.pv_accuracy_z3.push([         '', entry.pv.accuracy.z3 ]);
    }).addListener('end', function() {
        callback(data);

        var i = 0;

        if (featurePath) {
            var allFeatures = [];

            data.entries.forEach(function(entry) {
                var gmPath = resultPath + 'gm/good/' + entry.id + '.jpg.json';
                var gm = fs.existsSync(gmPath) ? JSON.parse(fs.readFileSync(gmPath)) : {};

                var features = {
                    //id: entry.id,

                    matches: entry.se.matches ? entry.se.matches.length : 0,
                    success: entry.se.success ? 1 : 0,
                    avg_confidence: entry.se.avg_confidence,
                    accuracy6: entry.se.accuracy.z2 == 0 ? 0 : 1,
                    accuracy7: entry.se.accuracy.z1 == 0 ? 0 : 1,

                    image_area: gm.size.width * gm.size.height,
                    image_resolution: gm['Resolution'] ? gm['Resolution'].substring(0, gm['Resolution'].indexOf('x')) : undefined,
                    image_quality: gm['JPEG-Quality'],
                    image_filesize: unit_k(gm['Filesize'])
                };

                //console.log(entry.se);
                //console.log(features);

                allFeatures.push(features);
            });

            fs.writeFileSync(featurePath, JSON.stringify(allFeatures, null, 2));
            console.log(allFeatures.length + ' entries written to ' + featurePath);
        }
    });
};

process.on('uncaughtException', function(err) {
    console.error(err.stack);
});

loadData(launchServer);