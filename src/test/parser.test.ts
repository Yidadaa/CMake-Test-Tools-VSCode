import { expect, describe, it } from "vitest";
import { equalFrom, parseCatch2FromText, Catch2TestCase } from "../parser";

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

    const expectResult: Catch2TestCase = {
      type: "SCENARIO",
      name: "1",
      range: {
        fromLine: 1,
        toLine: 8,
      },
      children: [
        {
          type: "GIVEN",
          name: "2",
          range: {
            fromLine: 2,
            toLine: 7,
          },
          children: [
            {
              type: "WHEN",
              name: "3",
              range: {
                fromLine: 3,
                toLine: 6,
              },
              children: [
                {
                  type: "THEN",
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

    const result = parseCatch2FromText(text);

    expect(result.length).toBe(1);
    expect(result).toEqual([expectResult]);
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
    const expectResult: Catch2TestCase[] = [
      {
        type: "SCENARIO",
        name: "1",
        range: {
          fromLine: 1,
          toLine: 6,
        },
        children: [
          {
            type: "GIVEN",
            name: "1.1",
            range: {
              fromLine: 2,
              toLine: 3,
            },
            children: [],
          },
          {
            type: "GIVEN",
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
        type: "SCENARIO",
        name: "2",
        range: {
          fromLine: 8,
          toLine: 9,
        },
        children: [],
      },
    ];

    const result = parseCatch2FromText(text);

    expect(result.length).toBe(2);
    expect(result).toEqual(expectResult);
  });

  it("Unformatted Input Scenario", () => {
    const text = `
SCENARIO( "1"    ) {
  GIVEN(   "  2" ) {}
}    
    `;

    const expectResult: Catch2TestCase[] = [
      {
        type: "SCENARIO",
        name: "1",
        range: { fromLine: 1, toLine: 3 },
        children: [
          {
            type: "GIVEN",
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

    expect(parseCatch2FromText(text)).toEqual(expectResult);
  });

  it("Bad Case: Unmatched Parentheses", () => {
    const text = `
SCENARIO("1") {
  GIVEN("
}    
    `;

    expect(() => parseCatch2FromText(text)).not.toThrowError();
  });

  it("Bad Case: Unmatched Parentheses Right", () => {
    const text = `
SCENARIO("1") {
  GIVEN(""))
}    
    `;

    expect(() => parseCatch2FromText(text)).not.toThrow();
  });

  it("Bad Case: Unmatched Brackets Right", () => {
    const text = `
SCENARIO("1") {
  GIVEN("") {}}}
}    
    `;

    expect(() => parseCatch2FromText(text)).not.toThrow();
  });
});
