import * as vscode from "vscode";

export let globalTerminal: vscode.Terminal;
export function runCommand(commandText: string) {
  if (!globalTerminal) {
    globalTerminal = vscode.window.createTerminal("CMake Test Tools");
    globalTerminal.show();
    globalTerminal.sendText(commandText);
  }
}
