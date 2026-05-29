# mutable_bigint 性能对比（公平版：C++ Google Benchmark + Java JMH）

| 语言 | 实现 | 工具 |
|------|------|------|
| C++ | `src/mutable_bigint.h` | [Google Benchmark](https://github.com/google/benchmark) |
| Java | JDK `java.math.MutableBigInteger` | [JMH](https://github.com/openjdk/jmh)（`out/java/jmh/`） |

**不使用** 项目内 `javamath/`。

## 依赖

```bash
brew install google-benchmark maven
cmake -S . -B build -DBUILD_BENCHMARKS=ON
cmake --build build
```

## 一键跑两边

```bash
bash out/run_benchmarks.sh
```

| 文件 | 内容 |
|------|------|
| `cpp_console.txt` / `cpp.json` | C++ 结果 |
| `jmh.json` / `jmh_console.txt` / `java.csv` | JMH 结果 |
| `comparison.csv` | 长表 |
| `comparison_pivot.csv` | 同场景 cpp vs java 并排 |

## 公平性说明（与旧手写 `nanoTime` 脚本的区别）

| 场景 | C++ | Java (JMH) |
|------|-----|------------|
| `BM_JarrayCtor` | 每次拷贝 `jarray` | 每次 `Arrays.copyOf` 再构造 |
| `BM_CopyAssign` | 复用 `dst`，`operator=` | 复用 `dst`，`copyValue` |
| `BM_Move*` | 真 `std::move` | 无 move；同工作量（每轮 `makeValue` + 拷贝） |
| `BM_Clear` / `BM_Reset` | `PauseTiming` 恢复非零，只计 clear/reset | `@Setup(Level.Iteration)` 恢复，只计 clear/reset |

旧脚本 `MutableBigIntBenchmark.java` 仍保留，仅作参考；请以 JMH 为准。

## 单独运行

```bash
./build/benchmark/mutable_bigint_bench
bash out/run_java_jmh.sh
```
