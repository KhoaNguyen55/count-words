# Table of Content

1. [Code documentation](#code-documentation)
2. [Report](#report)

# Code Documentation

# Report

## Description
    Completeness and clarity of problem definitions and summary of your approaches

## Code Structures
    Quality of the provided diagrams and explanation to show the overall structure of your code
    Satisfaction and completeness of the project requirements

## Instructions
**Note: because this code only work on linux system, the instruction only apply to linux.**

First clone the repository
```sh
git clone --recursive https://github.com/KhoaNguyen55/count-words.git
```

Go into the folder
```sh
cd count-words
```

Because of the unknown license of the files we were given to count the words of, it is not in this repo so you have to download and extract it.
```sh
# download the archive
wget https://corpus.canterbury.ac.nz/resources/calgary.tar.gz
# extract the files
tar -xf calgary.tar.gz -Cfiles paper1 paper2 progc progl progp trans bib
```

Compiles the programs

```sh
make multithread
make singlethread
```

run it
```sh
cd build
./multithread > output-multi.csv
./singlethread > output-single.csv
```

## Results
### Histogram of top 50 words:

![histrogram of top 50 words](./images/histogram.png "Top 50 words")

### Performance:
#### Single thread: 
**Memory usage**: Memory usage was analyzed with `valgrind --tool=massif`
- Process 0: peak cost: "12.8 MiB" heap "8.1 KiB" heap extra "0 B" stacks
- Process 1: peak cost: "1.6 MiB" heap "321 B" heap extra "0 B" stacks
- Process 2: peak cost: "1.6 MiB" heap "321 B" heap extra "0 B" stacks
- Process 3: peak cost: "3.2 MiB" heap "321 B" heap extra "0 B" stacks
- Process 4: peak cost: "3.2 MiB" heap "321 B" heap extra "0 B" stacks
- Process 5: peak cost: "3.2 MiB" heap "321 B" heap extra "0 B" stacks
- Process 6: peak cost: "3.2 MiB" heap "321 B" heap extra "0 B" stacks
- Process 7: peak cost: "1.6 MiB" heap "321 B" heap extra "0 B" stacks

**Cpu Usage**: Cpu performance was analyzed with `perf stat -r 5`

|Amount|Event|Variance|Note|
| - |      - |   - |   - |
193.80 msec | task-clock | 4.38% | 2.486  CPUs utilized
8785   | context-switches | 4.35% | 45.330  K/sec
84   | cpu-migrations | 10.44% | 433.434  /sec
10873   | page-faults | 0.07% | 56.104  K/sec
604805819   | cycles | 2.51% | 3.121  GHz
185127331   | stalled-cycles-frontend | 1.45% | 30.61%  frontend cycles idle
584177432   | instructions | 0.71% | 0.97 insn per cycle, 0.32 stalled cycles per insn
119858176   | branches | 0.76% | 618.460  M/sec
6182459   | branch-misses | 0.64% | 5.16%  of all branches

0.07995 +- 0.00226 seconds time elapsed  ( +-  2.82% )


#### Multi threads : 5 threads
**Memory usage**: Memory usage was analyzed with `valgrind --tool=massif`
- Process 0: peak cost: "12.8 MiB" heap "8.1 KiB" heap extra "0 B" stacks
- Process 1: peak cost: "1.7 MiB" heap "481 B" heap extra "0 B" stacks
- Process 2: peak cost: "1.7 MiB" heap "481 B" heap extra "0 B" stacks
- Process 3: peak cost: "3.3 MiB" heap "481 B" heap extra "0 B" stacks
- Process 4: peak cost: "3.3 MiB" heap "481 B" heap extra "0 B" stacks
- Process 5: peak cost: "3.3 MiB" heap "481 B" heap extra "0 B" stacks
- Process 6: peak cost: "3.3 MiB" heap "481 B" heap extra "0 B" stacks
- Process 7: peak cost: "1.7 MiB" heap "481 B" heap extra "0 B" stacks

**Cpu Usage**: Cpu performance was analyzed with `perf stat`
|Amount|Event|Variance|Note|
| - |      - |   - |   - |
| 289.86 msec | task-clock | 4.60% | 3.501  CPUs utilized |
| 11093   | context-switches | 6.26% | 38.270  K/sec
| 803   | cpu-migrations | 6.24% | 2.770  K/sec
| 11013   | page-faults | 0.09% | 37.994  K/sec
| 959741041 | cycles | 5.76% | 3.311  GHz
| 359238116 | stalled-cycles-frontend | 8.99% | 37.43%  frontend cycles idle
| 782356146 | instructions | 5.12% | 0.82  insn per cycle, 0.46  stalled cycles per insn
| 166391696 | branches | 6.04% | 574.043  M/sec
| 12176492 | branch-misses | 9.97% | 7.32%  of all branches

0.07217 +- 0.00440 seconds time elapsed  ( +-  6.09% )

<!-- Difference in performance between multiprocessing and multithreading -->
<!-- Discussion on the verification results -->

## Discussion of Findings
    Insightfulness of the discussion on project findings, limitations, and improvement suggestions.

