/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";

import { withPrefix } from "./constant";

export enum Commands {
  RunTest = "runTest",
  DiscoverTests = "discoverTests",
}

const commands = {
  [Commands.RunTest]: () => {
    vscode.window.showInformationMessage("Building Test...");
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
    console.log("create command", commandName);

    const commandItem = vscode.commands.registerCommand(
      withPrefix(commandName),
      commands[commandName as Commands]
    );

    ctx.subscriptions.push(commandItem);
  }
}
