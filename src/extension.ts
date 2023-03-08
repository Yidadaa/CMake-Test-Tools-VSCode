// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import * as vscode from "vscode";
import * as path from "path";

import { RunTestCodeLensProvider } from "./codelens";
import { debugTest, registerAllCommands, runTest } from "./commands";
import { GlobalTestStore, createRunnerFromFile } from "./runner";

function breakPathToFolders(targetPath: string) {
  const folders = [];

  for (
    let currentPath = targetPath;
    currentPath !== path.dirname(currentPath);
    currentPath = path.dirname(currentPath)
  ) {
    folders.push(path.basename(currentPath));
  }

  return folders.reverse();
}

class TestContext {
  testController: vscode.TestController;
  codeLensController: vscode.Disposable;
  lastTestItem: vscode.TestItem | undefined;

  constructor(private context: vscode.ExtensionContext) {
    this.testController = vscode.tests.createTestController(
      "CMake Tests",
      "Current File Tests"
    );

    this.testController.createRunProfile(
      "Run Test",
      vscode.TestRunProfileKind.Run,
      this.execTest.bind(this),
      true
    );

    this.testController.createRunProfile(
      "Debug Test",
      vscode.TestRunProfileKind.Debug,
      this.execTest.bind(this),
      true
    );

    this.codeLensController = vscode.languages.registerCodeLensProvider(
      { language: "cpp", scheme: "file" },
      new RunTestCodeLensProvider()
    );

    this.registerEvents();

    context.subscriptions.push(this.testController);
    context.subscriptions.push(this.codeLensController);
  }

  registerEvents() {
    vscode.workspace.onDidSaveTextDocument(this.addTestItemFromFile.bind(this));
    vscode.workspace.onDidOpenTextDocument(this.addTestItemFromFile.bind(this));
  }

  addTestItemFromFile(doc: vscode.TextDocument) {
    const item = createRunnerFromFile(this.testController, doc);
    if (!item) {
      return;
    }

    const relativePath = vscode.workspace.asRelativePath(doc.uri);

    let currentItem: { items: vscode.TestItemCollection } = this.testController;
    const dirs = breakPathToFolders(relativePath);

    const fileName = dirs.pop();
    item.label = fileName ?? item.label;

    for (const dir of dirs) {
      let nextItem = currentItem.items.get(dir);
      if (!nextItem) {
        const pathItem = this.testController.createTestItem(dir, dir, doc.uri);
        currentItem.items.add(pathItem);
        nextItem = pathItem;
      }

      currentItem = {
        items: nextItem.children,
      };
    }

    currentItem.items.add(item);
  }

  execTest(runRequest: vscode.TestRunRequest) {
    this.lastTestItem = runRequest.include?.at(0) ?? this.lastTestItem;
    const testItem = this.lastTestItem;
    const testCase = GlobalTestStore.get(testItem?.id ?? "");

    if (runRequest.profile?.kind === vscode.TestRunProfileKind.Run) {
      const currentTestRun = this.testController.createTestRun(runRequest);
      testItem && currentTestRun.started(testItem);

      runTest(testCase)
        .then((success) => {
          if (success) {
            testItem && currentTestRun.passed(testItem);
          } else {
            testItem && currentTestRun.failed(testItem, []);
          }
        })
        .finally(() => {
          currentTestRun.end();
        });
    } else if (runRequest.profile?.kind === vscode.TestRunProfileKind.Debug) {
      debugTest(testCase);
    }
  }

  createRunnerFromOpenedFiles() {
    vscode.workspace.textDocuments.forEach(this.addTestItemFromFile.bind(this));
  }

  dispose() {
    this.testController.dispose();
    this.codeLensController.dispose();
  }
}

export function activate(context: vscode.ExtensionContext) {
  const ctx = new TestContext(context);
  ctx.createRunnerFromOpenedFiles();
  registerAllCommands(context);
}

// This method is called when your extension is deactivated
export function deactivate() {}
