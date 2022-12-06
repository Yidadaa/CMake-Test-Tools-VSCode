import * as vscode from "vscode";
import { RunTestCodeLensProvider } from "./codelens";

export function createRunner(ctx: vscode.ExtensionContext) {
  const testController = vscode.tests.createTestController(
    "CMake Tests",
    "Current File Tests"
  );

  const item = testController.createTestItem("hello", "sample test 2");

  item.canResolveChildren = true;

  const child = testController.createTestItem("hello child", "child test");

  item.children.add(child);

  testController.items.add(item);

  testController.createRunProfile(
    "Run Test",
    vscode.TestRunProfileKind.Run,
    console.log,
    true
  );

  const codeLensController = vscode.languages.registerCodeLensProvider(
    { language: "cpp", scheme: "file" },
    new RunTestCodeLensProvider()
  );

  ctx.subscriptions.push(testController);
  ctx.subscriptions.push(codeLensController);
}
