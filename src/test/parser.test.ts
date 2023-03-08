import { expect, describe, it } from "vitest";
import {
  Catch2TestCase,
  equalFrom,
  parseTestCasesFromText,
  TestCase,
} from "../parser";

function exclude<T extends { children: T[] }>(
  testCase: T,
  fields: Array<keyof T>
) {
  fields.forEach((field) => delete testCase[field]);
  testCase.children.forEach((child) => exclude(child, fields));

  return testCase;
}

function excludeParent<T extends { parent?: T; children: T[] }>(cases: T[]) {
  return cases.map((v) => exclude(v, ["parent"]));
}

describe("Parser Test Suite", () => {
  it("Test equalFrom", () => {
    expect(equalFrom("abcd", 0, "abc")).toBe(true);
    expect(equalFrom("abcd", 3, "abc")).toBe(false);
  });

  it("Scenario Parse", () => {
    const text = `
SCENARIO("1") {
  GIVEN("2") {
    WHEN("3") {
      THEN("4") {
      }
    }
  }
}
    `;

    const expectResult: TestCase = {
      token: "SCENARIO",
      name: "1",
      range: {
        fromLine: 1,
        toLine: 8,
      },
      children: [
        {
          token: "GIVEN",
          name: "2",
          range: {
            fromLine: 2,
            toLine: 7,
          },
          children: [
            {
              token: "WHEN",
              name: "3",
              range: {
                fromLine: 3,
                toLine: 6,
              },
              children: [
                {
                  token: "THEN",
                  name: "4",
                  range: {
                    fromLine: 4,
                    toLine: 5,
                  },
                  children: [],
                },
              ],
            },
          ],
        },
      ],
    };

    const result = parseTestCasesFromText(text);

    expect(result.length).toBe(1);
    expect(excludeParent(result)).toEqual([expectResult]);
  });

  it("Parse Multi Scenarios and GWT", () => {
    const text = `
SCENARIO("1") {
  GIVEN("1.1") {
  }
  GIVEN("1.2") {
  }
}

SCENARIO("2") {
}
    `;
    const expectResult: TestCase[] = [
      {
        token: "SCENARIO",
        name: "1",
        range: {
          fromLine: 1,
          toLine: 6,
        },
        children: [
          {
            token: "GIVEN",
            name: "1.1",
            range: {
              fromLine: 2,
              toLine: 3,
            },
            children: [],
          },
          {
            token: "GIVEN",
            name: "1.2",
            range: {
              fromLine: 4,
              toLine: 5,
            },
            children: [],
          },
        ],
      },
      {
        token: "SCENARIO",
        name: "2",
        range: {
          fromLine: 8,
          toLine: 9,
        },
        children: [],
      },
    ];

    const result = parseTestCasesFromText(text);

    expect(result.length).toBe(2);
    expect(excludeParent(result)).toEqual(expectResult);
  });

  it("Unformatted Input Scenario", () => {
    const text = `
SCENARIO( "1"    ) {
  GIVEN(   "  2" ) {}
}    
    `;

    const expectResult: TestCase[] = [
      {
        token: "SCENARIO",
        name: "1",
        range: { fromLine: 1, toLine: 3 },
        children: [
          {
            token: "GIVEN",
            name: "  2",
            range: {
              fromLine: 2,
              toLine: 2,
            },
            children: [],
          },
        ],
      },
    ];

    expect(excludeParent(parseTestCasesFromText(text))).toEqual(expectResult);
  });

  it("Bad Case: Unmatched Parentheses", () => {
    const text = `
SCENARIO("1") {
  GIVEN("
}    
    `;

    expect(() => parseTestCasesFromText(text)).not.toThrowError();
  });

  it("Bad Case: Unmatched Parentheses Right", () => {
    const text = `
SCENARIO("1") {
  GIVEN(""))
}    
    `;

    expect(() => parseTestCasesFromText(text)).not.toThrow();
  });

  it("Bad Case: Unmatched Brackets Right", () => {
    const text = `
SCENARIO("1") {
  GIVEN("") {}}}
}    
    `;

    expect(() => parseTestCasesFromText(text)).not.toThrow();
  });

  it("Match second args of Macros", () => {
    const text = `
SCENARIO_METHOD(TestMethod, "1") {
  GIVEN("2") {}
}
    `;
    const expectResult = {
      token: "SCENARIO_METHOD",
      name: "1",
      range: {
        fromLine: 1,
        toLine: 3,
      },
      children: [
        {
          token: "GIVEN",
          name: "2",
          range: {
            fromLine: 2,
            toLine: 2,
          },
          children: [],
        },
      ],
    };
    expect(excludeParent(parseTestCasesFromText(text))).toEqual([expectResult]);
  });

  it("Match with brackets in name", () => {
    const text = `
      SCENARIO("test() and struct{}") {}
    `;

    const expectResult: Catch2TestCase = {
      token: "SCENARIO",
      name: "test() and struct{}",
      range: {
        fromLine: 1,
        toLine: 1,
      },
      children: [],
    };

    const parsed = parseTestCasesFromText(text);
    expect(excludeParent(parsed)).toEqual([expectResult]);
  });

  it("Match well when existing front brackets", () => {
    const text = `
    {}
      SCENARIO("test() and struct{}") {}
    `;

    const expectResult: Catch2TestCase = {
      token: "SCENARIO",
      name: "test() and struct{}",
      range: {
        fromLine: 2,
        toLine: 2,
      },
      children: [],
    };

    const parsed = parseTestCasesFromText(text);
    expect(excludeParent(parsed)).toEqual([expectResult]);
  });
});
