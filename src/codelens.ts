import * as vscode from "vscode";
import { parseTestCasesFromText } from "./parser";

export class RunTestCodeLensProvider implements vscode.CodeLensProvider {
  async provideCodeLenses(
    document: vscode.TextDocument,
    token: vscode.CancellationToken
  ): Promise<vscode.CodeLens[] | null | undefined> {
    const testCases = parseTestCasesFromText(document.getText());
    let lens: vscode.CodeLens[] = [];

    let command: vscode.Command = {
      command: "cmake-test-tools.runCurrentTest",
      title: "Run",
    };

    let debugCommand: vscode.Command = {
      command: "cmake-test-tools.runCurrentTest",
      title: "Debug",
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
        lens.push(new vscode.CodeLens(lensRange, command));
        lens.push(new vscode.CodeLens(lensRange, debugCommand));
      }

      testCase?.children.forEach((item) => testCases.push(item));
    }

    return lens;
  }
}
