/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";
import { Commands, GlobalVars } from "./constant";
import { runCommand } from "./terminal";
import { TestCase } from "./parser";
import * as fs from "fs";

const commands = {
  [Commands.RunTest]: (testCase?: TestCase) => {
    GlobalVars.currentTestCase = testCase;

    vscode.commands.executeCommand("cmake.build").then((res) => {
      vscode.commands
        .executeCommand("cmake.getLaunchTargetPath")
        .then((target) => {
          vscode.window.showInformationMessage(target as string);

          runCommand([target, ...GlobalVars.getTestArgs()].join(" "));
        });
    });
  },
  [Commands.DebugTest]: (testCase: TestCase) => {
    GlobalVars.currentTestCase = testCase;
    vscode.debug.startDebugging(undefined, {
      type: "lldb",
      request: "launch",
      name: "Debug CMake Target Test",
      program: "${command:cmake.launchTargetPath}",
      args: GlobalVars.getTestArgs(),
      cwd: "${workspaceFolder}",
    });
  },
  [Commands.GetTestParam]: () => {
    return GlobalVars.getTestArgs();
  },
  [Commands.DiscoverTests]: () => {
    vscode.window.withProgress(
      {
        location: vscode.ProgressLocation.Notification,
        title: "Discovering Tests...",
        cancellable: true,
      },
      (progress, token) => {
        token.onCancellationRequested(() => {
          vscode.window.showInformationMessage("You canceled discovering.");
        });

        return new Promise<void>((resolve) => {
          function fakeUpdate(val = 0) {
            if (val < 100) {
              setTimeout(() => {
                fakeUpdate(val + 10);
              }, 200);
            } else {
              resolve();
            }

            progress.report({ message: `${val}%`, increment: 10 });
          }

          fakeUpdate();
        });
      }
    );
  },
};

export function registerAllCommands(ctx: vscode.ExtensionContext) {
  for (const commandName in commands) {
    const commandItem = vscode.commands.registerCommand(
      commandName,
      commands[commandName as Commands]
    );

    ctx.subscriptions.push(commandItem);
  }
}
