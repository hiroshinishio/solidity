// SPDX-License-Identifier: GPL-3.0
pragma solidity >=0.0;
contract A {
    uint public transient x;
    uint public y;
    int private transient z;
    int private w;
    address external transient addr;
    address external d;
    bool transient b;
    bool c;
    int constant i = 1;
    int transient constant j = 2;
    uint immutable k;
    uint transient immutable l;
}
