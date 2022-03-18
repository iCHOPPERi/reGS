# reGS: reverse-engineered GoldSrc engine

This is private repository of reGS source code.

## Goal

We want to keep compatibility with original GoldSrc parts (network/GameUI and other libraries). Also, we try to fix bugs of the original engine and its components.

## Building

Currently this project supports only Windows.

### Visual Studio 2019 & Visual Studio 2022
1. Install [Visual Studio 2019](https://my.visualstudio.com/Downloads?q=Visual%20Studio%20Community%202019) or [Visual Studio 2022](https://visualstudio.microsoft.com/vs/preview/vs2022/#download-preview). In the Visual Studio Installer, select Desktop Development for C++.
2. Open Visual Studio.
3. On the starting screen, click "Clone or check out code".
4. Enter `https://github.com/ScriptedSnark/reGS.git` and press the Clone button. Wait for the process to finish.
5. You can build the workspace using Build→Build All. If you want to build only engine or something else, right click on wanted project (for example, `launcher`) and select Build.
