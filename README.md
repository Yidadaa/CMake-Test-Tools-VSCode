# CMake Test Tools Extension for VSCode
This extension is an alternative of [TestMate Extension](https://github.com/matepek/vscode-catch2-test-adapter), supports Google Test and Catch2.

![banner.png](./res/doc/banner.png)

## Preview
> Here is a screen recording GIF file. Please be patient if your network connection is slow.

![demo.gif](res/doc/demo.gif)

## Features
- Blazing fast discovering tests from opening files, but not from whole project, which makes it faster than TestMate in huge projects;
- Add `run` and `debug` inline Code Lens actions to document;
- Integrated with [Offical CMake Tools Extension](https://github.com/microsoft/vscode-cmake-tools).

## Requirements
All requirements are same as [CMake Tools Extension](https://github.com/microsoft/vscode-cmake-tools).


## Usage
Before click `run` and `debug` button, please make sure that the CMake build target and CMake launch target in the status bar are consistent, and that the target include the file of the case you want to run.

![usage](./res/doc/tip.jpg)

## Extension Settings
```
{
  "cmake-test-tools.debugType": {
    "type": "string",
    "default": "lldb", // You can change to other debuggers
    "markdownDescription": "The type for debug command."
  }
}
```

## Known Issues
If you encounter any issues while using, please submit an issue [here](https://github.com/Yidadaa/CMake-Test-Tools-VSCode/issues).

## Release Notes
See [change log](./CHANGELOG.md);

# License
- Licensed by [Anti 996 License](https://github.com/kattgu7/Anti-996-License)