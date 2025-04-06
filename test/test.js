const assert = require("assert");
const SMInverter = require("../");

describe("SMInverter", function () {
  it("should create an instance", function () {
    const inverter = new SMInverter();
    assert.strictEqual(typeof inverter, "object");
    assert.strictEqual(typeof inverter.initialize, "function");
  });

  // Note: Full integration tests would require actual hardware
});
