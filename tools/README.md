# tools/

Developer tooling — not part of the ROM.

## `ai_sim.c` — AI strength harness

A desktop (plain C, no Game Boy) port of the board rules and CPU AI from
`src/main.c`. It plays the three difficulties against each other in a round-robin
— each pairing both ways to cancel first-move bias — and prints win counts. This
is how the difficulty ladder was tuned and is the regression test for it: if you
change the AI in `main.c`, mirror the change here and re-run to confirm the
levels stay ordered.

```sh
cc -O2 -o ai_sim ai_sim.c
./ai_sim
```

Expected (clean ladder, no blowouts):

```
Easy    vs Normal  :  Normal favored   (~60/40)
Easy    vs Hard    :  Hard   favored   (~66/34)
Normal  vs Hard    :  Hard   favored   (~56/44)
```
