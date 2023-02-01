import * as vscode from "vscode";
import { parseTestCasesFromText } from "./parser";
import { Commands } from "./constant";

export class RunTestCodeLensProvider implements vscode.CodeLensProvider {
  async provideCodeLenses(
    document: vscode.TextDocument,
    token: vscode.CancellationToken
  ): Promise<vscode.CodeLens[] | null | undefined> {
    const testCases = parseTestCasesFromText(document.getText());
    let lens: vscode.CodeLens[] = [];

    let command: vscode.Command = {
      command: Commands.RunTest,
      title: "Run",
      arguments: [],
    };

    let debugCommand: vscode.Command = {
      command: Commands.DebugTest,
      title: "Debug",
      arguments: [],
    };

    while (testCases.length > 0) {
      const testCase = testCases.pop();
      const testRange = testCase?.range;

      if (testRange) {
        const lensRange = new vscode.Range(
          testRange.fromLine,
          0,
          testRange.toLine,
          0
        );
        const args = testCase;
        lens.push(
          new vscode.CodeLens(lensRange, { ...command, arguments: [args] })
        );
        lens.push(
          new vscode.CodeLens(lensRange, { ...debugCommand, arguments: [args] })
        );
      }

      testCase?.children.forEach((item) => testCases.push(item));
    }

    return lens;
  }
}
