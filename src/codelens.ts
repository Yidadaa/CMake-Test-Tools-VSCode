import * as vscode from "vscode";

export class RunTestCodeLensProvider implements vscode.CodeLensProvider {
  async provideCodeLenses(
    document: vscode.TextDocument,
    token: vscode.CancellationToken
  ): Promise<vscode.CodeLens[] | null | undefined> {
    let topOfDoc = new vscode.Range(10, 0, 0, 0);

    let command: vscode.Command = {
      command: "cmake-test-tools.runCurrentTest",
      title: "Run",
    };

    return [new vscode.CodeLens(topOfDoc, command)];
  }
}
