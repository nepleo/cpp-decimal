# Java 对拍（mutable_bigint ↔ JDK）

使用 **本机 JDK** 里的 `java.math.MutableBigInteger`（`$JAVA_HOME/lib/src.zip` 中的官方实现），**不**编译项目内的 `javamath/`。

Harness 通过 `--patch-module java.base` 以同包 `java.math` 注入，从而访问 package-private 的 `MutableBigInteger`。

## 构建并运行

```bash
bash out/run_java_harness.sh ping
bash out/run_java_harness.sh default_ctor
bash out/run_java_harness.sh ctor_int 42
```

首次运行会用 `javac --patch-module java.base=out/java/jdk-patch` 编译  
[`out/java/jdk-patch/java/math/MutableBigIntHarness.java`](java/jdk-patch/java/math/MutableBigIntHarness.java)。

需要已安装 JDK，并建议设置 `JAVA_HOME`（macOS 可用 `/usr/libexec/java_home`）。

## C++ 测试中启用对比

```bash
cd build
cmake --build .
MUTABLE_BIGINT_JAVA=1 ctest --output-on-failure -R 'MutableBigInt.*Java'
```

未设置 `MUTABLE_BIGINT_JAVA` 时，`*Java` 用例跳过与 JDK 的比对（纯 C++ 断言仍运行）。

## 文件说明

| 路径 | 说明 |
|------|------|
| `out/java/jdk-patch/java/math/MutableBigIntHarness.java` | 对拍入口（`package java.math`） |
| `out/java/jdk-patch/java/math/MutableBigIntHarness.class` | 编译产物（与 .java 同目录，供 patch-module 加载） |
| `out/run_java_harness.sh` | 编译 + 运行脚本 |

JDK 参考源码位置（只读）：`$JAVA_HOME/lib/src.zip` → `java.base/java/math/MutableBigInteger.java`

## Benchmark

见 [`benchmark/README.md`](benchmark/README.md)。运行：`bash out/run_benchmarks.sh`
