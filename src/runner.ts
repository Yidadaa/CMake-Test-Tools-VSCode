import * as vscode from "vscode";
import { RunTestCodeLensProvider } from "./codelens";
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
  const entry = testController.createTestItem(
    doc.uri.toString(),
    doc.fileName,
    doc.uri
  );
  const cases = parseTestCasesFromText(doc.getText());
  buildTestItemFromTestCases(testController, cases, doc.uri).forEach((item) =>
    entry.children.add(item)
  );

  testController.items.add(entry);
}

export function createRunner(ctx: vscode.ExtensionContext) {
  const testController = vscode.tests.createTestController(
    "CMake Tests",
    "Current File Tests"
  );

  const openedFiles = vscode.workspace.textDocuments;

  for (const doc of openedFiles) {
    createRunnerFromFile(testController, doc);
  }

  function runTest() {
    console.log("hello", arguments);
  }

  testController.createRunProfile(
    "Run Test",
    vscode.TestRunProfileKind.Run,
    runTest,
    true
  );

  testController.createRunProfile(
    "Debug Test",
    vscode.TestRunProfileKind.Debug,
    runTest,
    true
  );

  const codeLensController = vscode.languages.registerCodeLensProvider(
    { language: "cpp", scheme: "file" },
    new RunTestCodeLensProvider()
  );

  ctx.subscriptions.push(testController);
  ctx.subscriptions.push(codeLensController);
}
