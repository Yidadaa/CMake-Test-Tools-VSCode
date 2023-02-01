/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";

export const EXTENSION_NAME = "cmake-test-tools";

export enum Commands {
  DebugTest = "cmake-test-tools.debugTest",
  RunTest = "cmake-test-tools.runTest",
  DiscoverTests = "cmake-test-tools.discoverTests",
  GetTestParam = "cmake-test-tools.getTestParam",
}
