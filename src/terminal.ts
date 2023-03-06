import * as vscode from "vscode";

export let globalTerminal: vscode.Terminal;
export function runCommand(commandText: string) {
  if (globalTerminal) {
    globalTerminal.dispose();
  }
  globalTerminal = vscode.window.createTerminal("CMake Test Tools");
  globalTerminal.show();
  globalTerminal.sendText(commandText);
}

/**
 * return a shell compatible format
 * @param args
 * @copyright modified from: https://github.com/xxorax/node-shell-escape/blob/master/shell-escape.js
 * @returns
 */
function escapeShellArgs(args: string[]) {
  const ret: string[] = [];

  args.forEach(function (arg) {
    if (/[^A-Za-z0-9_\/:=-]/.test(arg)) {
      arg = "'" + arg.replace(/'/g, "'\\''") + "'";
      arg = arg
        .replace(/^(?:'')+/g, "") // unduplicate single-quote at the beginning
        .replace(/\\'''/g, "\\'"); // remove non-escaped single-quote if there are enclosed between 2 escaped
    }
    ret.push(arg);
  });

  return ret.join(" ");
}
