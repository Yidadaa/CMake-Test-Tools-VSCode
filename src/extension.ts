// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import * as vscode from "vscode";
import * as path from "path";

import { RunTestCodeLensProvider } from "./codelens";
import { registerAllCommands } from "./commands";
import { createRunnerFromFile } from "./runner";

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

  constructor(private context: vscode.ExtensionContext) {
    this.testController = vscode.tests.createTestController(
      "CMake Tests",
      "Current File Tests"
    );

    this.testController.createRunProfile(
      "Run Test",
      vscode.TestRunProfileKind.Run,
      this.runTest.bind(this),
      true
    );

    this.testController.createRunProfile(
      "Debug Test",
      vscode.TestRunProfileKind.Debug,
      this.runTest.bind(this),
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

  runTest() {
    console.log("hello", arguments);
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
