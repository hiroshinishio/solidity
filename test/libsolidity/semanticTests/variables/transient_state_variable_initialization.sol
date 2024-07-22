contract C {
    uint transient public x = 2;

    constructor() {
        x = 1;
    }

}
// ====
// EVMVersion: >=cancun
// compileViaYul: true
// ----
// constructor() ->
// gas legacy: 77771
// gas legacy code: 23600
// x() -> 0
