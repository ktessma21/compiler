- %newVar2 is in **in** but not **out** — it dies here, it's the last use (consumed by the load).
- %newVar3 is in **out** but not **in** — it's born here, defined by this instruction.




- problem could be call instruction pure has call_node pure
- not working with the already existing call. 