# The Sneaky Mole

A console whack-a-mole game in C++17 where **the mole is the AI**. It watches where you swing and uses **Laplace's rule of succession** to hide where you're least likely to hit. The project also includes a small **red-team loop**: an AttackBot that tries to beat the mole, a measurement of how often it wins, and a patched mole that closes the weakness it found.

## How the mole learns

For each of the 9 holes, the mole estimates the chance you'll swing there next:

```
P(you swing at hole i) = (swings at hole i + 1) / (rounds so far + 2)
```

The mole commits to a hole **before** you swing, using only past rounds. There are two versions:

| Mole | Hiding rule |
|---|---|
| **Classic** | Hide in a least-swung hole (ties broken at random). Explore a random hole 20% of the time. |
| **Unpredictable** | Pick any hole at random, weighted by `1 / P(you swing there)`. Holes you rarely hit are more likely, but nothing is certain. |

## Red-team results

The AttackBot is a white-box attacker: it knows the mole's rule and uses the same swing history the player can see to predict where the mole is hiding. It never sees the mole's actual choice. Each strategy played 10,000 full games (15 rounds, 3 whacks to win):

| Attack strategy | Classic | Unpredictable |
|---|---|---|
| Always swing at the same hole | 0.7% | 0.6% |
| Swing at random | 22.2% | 22.5% |
| **AttackBot** | **71.3%** | **34.1%** |

- Both moles learn your habits: repeating one hole almost never wins.
- The Classic mole is too predictable. Anyone who knows the rule wins 71% of the time.
- The Unpredictable mole still avoids your habits, but knowing its rule only gets an attacker to 34%, close to random guessing.

## Features

- **Play** against either mole, with an option to show or hide the mole's reasoning and Laplace calculations each round.
- **Hints:** type `H` (up to 2 times per game) to see the AttackBot's prediction of where the mole is hiding.
- **Simulation mode:** a computer player swings at hole 5 with a hidden true probability of 40%, to check that the mole's estimate converges to the true value.
- **Red-team mode:** runs the table above.

! How to run

Visual Studio: open `SneakyMole.sln` and press `Ctrl + F5`.

**Command line:**
```
g++ -std=c++17 -Wall -Wextra SneakyMole/SneakyMole.cpp -o SneakyMole
./SneakyMole
```

Then choose `1` (play), `2` (simulation), or `3` (red-team test).

! What I learned

- A model can be mathematically correct and still be easy to exploit. The Classic mole's estimates were right, but its decision rule was predictable.
- Measuring an attack first (71%) made it clear what to fix, and re-running the same attack (34%) showed whether the fix worked.
- With small amounts of data, estimates are noisy. In simulation, 100 rounds landed anywhere from about 0.32 to 0.45 for a true probability of 0.40.

! Limitations

- The AttackBot is greedy: it only looks one round ahead. A smarter attacker that plans several rounds ahead might do better against the Unpredictable mole.
- Each hole's Laplace estimate is a separate yes/no estimate, so the nine values don't add up to 100%. The ranking is the same as a 9-way estimate.

! Background

Started as a class assignment on Laplace's rule of succession (from *Algorithms to Live By*, Chapter 6), then extended with the hint feature and the red-team loop.