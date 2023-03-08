import * as vscode from "vscode";
import { parseTestCasesFromText, TestCase } from "./parser";

// eslint-disable-next-line @typescript-eslint/naming-convention
export const GlobalTestStore = {
  testStore: new Map<string, TestCase>(),
  fileStore: new Map<string, Set<string>>(),
  insert(fileId: string, testId: string, testCase: TestCase) {
    if (!this.fileStore.has(fileId)) {
      this.fileStore.set(fileId, new Set());
    }
    this.fileStore.get(fileId)!.add(testId);
    this.testStore.set(testId, testCase);
  },
  get(testId: string) {
    return this.testStore.get(testId);
  },
  remove(fileId: string, testId: string) {
    this.fileStore.get(fileId)?.delete(testId);
    this.testStore.delete(testId);
  },
  clear(fileId: string) {
    this.fileStore.get(fileId)?.forEach((id) => this.testStore.delete(id));
    this.fileStore.delete(fileId);
  },
};

export function buildIdFromTestCase(fileName: string, testCase: TestCase) {
  return `${fileName}:${testCase.token}:${testCase.name}:${testCase.range?.fromLine}-${testCase.range?.toLine}`;
}

export function buildTestItemFromTestCases(
  controller: vscode.TestController,
  cases: TestCase[],
  uri: vscode.Uri
) {
  const uriId = uri.toString();

  // every time we parse tests from file, we clear old tests first
  GlobalTestStore.clear(uriId);

  function addTestItem(testCase: TestCase) {
    const id = buildIdFromTestCase(uriId, testCase);

    // update test cases store, we will use it when running test from test explorer
    GlobalTestStore.insert(uriId, id, testCase);

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

    if (cases.length > 0) {
      return entry;
    }
  }
}
