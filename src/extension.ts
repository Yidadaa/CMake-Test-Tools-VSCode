// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import * as vscode from "vscode";
import { registerAllCommands } from "./commands";
import { createRunner } from "./runner";

// This method is called when your extension is activated
// Your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {
  registerAllCommands(context);
  createRunner(context);

  setTimeout(() => {
    const docs = vscode.workspace.textDocuments;

    for (const doc of docs) {
      console.log("open files", doc.fileName);
    }

    for (const doc of vscode.window.visibleTextEditors) {
      console.log("window files", doc.document.fileName);
    }
  }, 100);
}

// This method is called when your extension is deactivated
export function deactivate() {}
