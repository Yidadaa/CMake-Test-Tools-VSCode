const CASE_NAMES = ["SCENARIO", "GIVEN", "WHEN", "THEN"] as const;
type CaseName = typeof CASE_NAMES[number];

export interface TestCaseRange {
  fromLine: number;
  toLine: number;
}

export interface Catch2TestCase {
  type: CaseName;
  name: string;
  range?: TestCaseRange;
  children: Catch2TestCase[];
}

export function equalFrom(s: string, pos: number, target: string) {
  let isEqual = s.length - pos >= target.length;

  for (let i = 0; i < target.length; ++i) {
    isEqual &&= s.at(pos + i) === target.at(i);
  }

  return isEqual;
}

export function parseCatch2FromText(text: string): Catch2TestCase[] {
  const retCase: Catch2TestCase = {
    type: "SCENARIO",
    name: "dummy",
    children: [],
  };

  type StackItem = {
    bracketCount: number;
    case: Catch2TestCase;
  };

  // Use stack to match bracketks
  const st: StackItem[] = [
    {
      bracketCount: 0,
      case: retCase,
    },
  ];

  // Parse and Match brackets
  let lineCount = 0;
  for (let i = 0; i < text.length; ++i) {
    if (text[i] === "\n") {
      lineCount += 1;
    } else if (text[i] === "{") {
      st.at(-1)!.bracketCount += 1;
    } else if (text[i] === "}") {
      st.at(-1)!.bracketCount -= 1;

      // Match to end of bracket, should pop from stack
      if (st.at(-1)?.bracketCount === 0) {
        if (st.at(-1)?.case.range) {
          st.at(-1)!.case.range!.toLine = lineCount;
        }
        st.pop();
      }
    } else {
      for (const name of CASE_NAMES) {
        if (!equalFrom(text, i, name)) {
          continue;
        }

        // Skip token
        i += name.length;

        const childCase: Catch2TestCase = {
          type: name,
          name: "",
          range: {
            fromLine: lineCount,
            toLine: lineCount,
          },
          children: [],
        };

        // Match name in next ()
        while (
          i < text.length &&
          text[i] !== ")" &&
          text[i] !== "{" &&
          text[i] !== "}"
        ) {
          childCase.name += text[i];
          i += 1;
        }

        const matches = childCase.name.match(/"(.*)"/);
        childCase.name = matches ? matches.at(1) ?? "" : "";
        st.at(-1)!.case.children.push(childCase);
        st.push({
          bracketCount: 0,
          case: childCase,
        });
      }
    }
  }

  return retCase.children;
}
