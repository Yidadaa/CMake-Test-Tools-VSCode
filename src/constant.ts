/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";

export const EXTENSION_NAME = "cmake-test-tools";
export const withPrefix = (s: string) => [EXTENSION_NAME, s].join(".");
