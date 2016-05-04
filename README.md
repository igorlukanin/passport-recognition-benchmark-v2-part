### Recognition

1. Put directories with images to `data`

2. Run recognition with SmartEngines:
```
cd recipes\smartengines
SmartEnginesRecognizer.exe
```

Source code for this executable is here: `src\SmartEnginesRecognizer\Program.cpp`.

Images are put to `data\image-pack-name` (e.g. `data\good`), JSON files with results are output to `data\image-pack-name\image-id.jpg.json`.

### Benchmarking

1. Put offline recognition data to `data\good.csv`.

2. Run `npm install && node src/app.js`, then browse `http://localhost:3000`.