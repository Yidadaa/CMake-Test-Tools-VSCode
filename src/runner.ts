import * as vscode from "vscode";
import { parseTestCasesFromText, TestCase } from "./parser";

export function buildIdFromTestCase(fileName: string, testCase: TestCase) {
  return `${fileName}:${testCase.token}:${testCase.name}:${testCase.range?.fromLine}-${testCase.range?.toLine}`;
}

export function buildTestItemFromTestCases(
  controller: vscode.TestController,
  cases: TestCase[],
  uri: vscode.Uri
) {
  function addTestItem(testCase: TestCase) {
    const id = buildIdFromTestCase(uri.toString(), testCase);
    const targetLine = testCase.range?.fromLine ?? 0;
    const item = controller.createTestItem(
      id,
      testCase.token + ": \t" + testCase.name,
      uri.with({ fragment: (targetLine + 1).toString() })
    );
    item.canResolveChildren = testCase.children.length > 0;
    item.sortText = targetLine.toString().padStart(10, "0");

    testCase.children.forEach((childCase) =>
      item.children.add(addTestItem(childCase))
    );

    return item;
  }

  return cases.map((testCase) => addTestItem(testCase));
}

export function createRunnerFromFile(
  testController: vscode.TestController,
  doc: vscode.TextDocument
) {
  if (doc.languageId === "cpp") {
    const entry = testController.createTestItem(
      doc.uri.toString(),
      doc.fileName,
      doc.uri
    );
    const cases = parseTestCasesFromText(doc.getText());
    buildTestItemFromTestCases(testController, cases, doc.uri).forEach((item) =>
      entry.children.add(item)
    );
    return entry;
  }
}
