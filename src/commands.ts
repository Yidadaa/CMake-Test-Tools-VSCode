/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";
import { Commands, Configurations, EXTENSION_NAME, GlobalVars } from "./constant";
import { runCommand } from "./terminal";
import { TestCase } from "./parser";
import * as fs from "fs";

export async function runTest(testCase?: TestCase) {
  GlobalVars.currentTestCase = testCase;

  // build target first
  const buildExitCode: number = await vscode.commands.executeCommand(
    "cmake.build"
  );

  if (buildExitCode !== 0) {
    return;
  }

  // get launch target path
  const target = await vscode.commands.executeCommand(
    "cmake.getLaunchTargetPath"
  );

  vscode.tasks.executeTask({
    definition: { type: "" },
    scope: undefined,
    name: "Run Test Case",
    isBackground: false,
    source: "cmake-test-tools",
    execution: new vscode.ShellExecution(
      target as string,
      GlobalVars.getTestArgs()
    ),
    presentationOptions: {},
    problemMatchers: [],
    runOptions: {},
  });

  return new Promise<boolean>((res) => {
    vscode.tasks.onDidEndTaskProcess((e) => {
      res(e.exitCode === 0);
    });
  });
}

export function debugTest(testCase?: TestCase) {
  GlobalVars.currentTestCase = testCase;
  let configuration = vscode.workspace.getConfiguration(EXTENSION_NAME);
  vscode.debug.startDebugging(undefined, {
    type: configuration.get(Configurations.DebugType) ?? "lldb",
    request: "launch",
    name: "Debug CMake Target Test",
    program: "${command:cmake.launchTargetPath}",
    args: GlobalVars.getTestArgs(),
    cwd: "${fileWorkspaceFolder}",
  });
}

const commands = {
  [Commands.RunTest]: runTest,
  [Commands.DebugTest]: debugTest,
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
