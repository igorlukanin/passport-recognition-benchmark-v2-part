### Recognition

1. Put directories with images to `data`

2. Run recognition with PassportVision:
```
cd recipes\passportvision
PassportVisionRecognizer.exe
```

3. Also run recognition with SmartEngines:
```
cd recipes\smartengines
SmartEnginesRecognizer.exe
```

4. Also run image metadata analysis

[Download](http://sourceforge.net/projects/graphicsmagick/files/graphicsmagick-binaries/1.3.23/) & install GraphicsMagick.
```
npm install
node src/gm.js
// Runs at least [image count] seconds
// Results are put to results/gm
```