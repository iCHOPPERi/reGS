![regs](https://user-images.githubusercontent.com/51358194/160087464-9220649b-e09f-40fb-aeb2-0cb9baad7aef.png)

# reGS: reverse-engineered GoldSrc engine

This is the repository of reGS source code. It was private before. Why I open-sourced reGS? Main reason, I was tired. I had some health problems due to active development of the engine. **Based on [GoldSourceRebuild](https://github.com/Triang3l/GoldSourceRebuild) repository.**

## Goal

We want to keep compatibility with original GoldSrc parts (network/GameUI and other libraries). Also, we try to fix bugs of the original engine and its components.
Third, improve GoldSrc by adding some features and improving already existing (for example, Miles Sound System to FMOD, OpenGL shaders).

## Credits

Thanks to [1Doomsayer](https://github.com/1Doomsayer) (the real skillful gigachad)! He helped me to learn reverse-engineering and also he made sound/MP3 system.

Thanks to [xWhitey](https://github.com/xWhitey) for reverse-engineering collaboration when reGS was private.

Thanks to [ReHLDS](https://github.com/dreamstalker/rehlds) repository for helping in reverse-engineering server part/network and etc.

## Building

Currently this project supports only Windows.

### Visual Studio 2019 & Visual Studio 2022
1. Install [Visual Studio 2019](https://my.visualstudio.com/Downloads?q=Visual%20Studio%20Community%202019) or [Visual Studio 2022](https://visualstudio.microsoft.com/vs/preview/vs2022/#download-preview). In the Visual Studio Installer, select Desktop Development for C++.
2. Open Visual Studio.
3. On the starting screen, click "Clone or check out code".
4. Enter `https://github.com/ScriptedSnark/reGS.git` and press the Clone button. Wait for the process to finish.
5. Change Debug to Release (debug build currently doesn't work). You can build the workspace using Build→Build All. If you want to build only engine or something else, right click on wanted project (for example, `launcher`) and select Build.
