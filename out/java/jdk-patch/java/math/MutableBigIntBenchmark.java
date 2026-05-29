package java.math;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Microbenchmark for JDK {@link MutableBigInteger} (same scenarios as C++ Google Benchmark).
 * Output: CSV columns name,limbs,iterations,ns_per_op
 */
public final class MutableBigIntBenchmark {

    private static final int WARMUP_ROUNDS = 3;
    private static int bench_iters = 2_000_000;

    public static void main(String[] args) {
        if (args.length > 0) {
            bench_iters = Integer.parseInt(args[0]);
        }

        List<String[]> rows = new ArrayList<>();
        rows.add(new String[] {"name", "limbs", "iterations", "ns_per_op"});

        benchDefaultCtor(rows);
        benchUintCtor(rows);
        for (int limbs : new int[] {1, 4, 16, 64}) {
            benchJarrayCtor(rows, limbs);
            benchCopyCtor(rows, limbs);
            benchCopyAssign(rows, limbs);
            benchMoveCtor(rows, limbs);
            benchMoveAssign(rows, limbs);
        }
        for (int limbs : new int[] {1, 16, 64}) {
            benchClear(rows, limbs);
            benchReset(rows, limbs);
        }

        PrintStream out = System.out;
        for (String[] row : rows) {
            out.println(String.join(",", row));
        }
    }

    private static void benchDefaultCtor(List<String[]> rows) {
        Runnable op = () -> {
            MutableBigInteger n = new MutableBigInteger();
            blackhole(n);
        };
        long ns = measure(op);
        rows.add(row("BM_DefaultCtor", 0, bench_iters, ns));
    }

    private static void benchUintCtor(List<String[]> rows) {
        Runnable op = () -> {
            MutableBigInteger n = new MutableBigInteger(0xdeadbeef);
            blackhole(n);
        };
        long ns = measure(op);
        rows.add(row("BM_UintCtor", 0, bench_iters, ns));
    }

    private static void benchJarrayCtor(List<String[]> rows, int limbs) {
        int[] mag = makeMag(limbs);
        Runnable op = () -> {
            MutableBigInteger n = new MutableBigInteger(mag);
            blackhole(n);
        };
        long ns = measure(op);
        rows.add(row("BM_JarrayCtor", limbs, bench_iters, ns));
    }

    private static void benchCopyCtor(List<String[]> rows, int limbs) {
        MutableBigInteger src = makeValue(limbs);
        Runnable op = () -> {
            MutableBigInteger n = new MutableBigInteger(src);
            blackhole(n);
        };
        long ns = measure(op);
        rows.add(row("BM_CopyCtor", limbs, bench_iters, ns));
    }

    private static void benchCopyAssign(List<String[]> rows, int limbs) {
        MutableBigInteger src = makeValue(limbs);
        MutableBigInteger dst = new MutableBigInteger();
        Runnable op = () -> {
            dst.copyValue(src);
            blackhole(dst);
        };
        long ns = measure(op);
        rows.add(row("BM_CopyAssign", limbs, bench_iters, ns));
    }

    private static void benchMoveCtor(List<String[]> rows, int limbs) {
        Runnable op = () -> {
            MutableBigInteger src = makeValue(limbs);
            MutableBigInteger dst = new MutableBigInteger(src);
            blackhole(dst);
            blackhole(src);
        };
        long ns = measure(op);
        rows.add(row("BM_MoveCtor", limbs, bench_iters, ns));
    }

    private static void benchMoveAssign(List<String[]> rows, int limbs) {
        Runnable op = () -> {
            MutableBigInteger src = makeValue(limbs);
            MutableBigInteger dst = new MutableBigInteger();
            dst.copyValue(src);
            blackhole(dst);
            blackhole(src);
        };
        long ns = measure(op);
        rows.add(row("BM_MoveAssign", limbs, bench_iters, ns));
    }

    private static void benchClear(List<String[]> rows, int limbs) {
        final MutableBigInteger[] box = new MutableBigInteger[] {makeValue(limbs)};
        Runnable setup = () -> box[0] = makeValue(limbs);
        Runnable op = () -> {
            box[0].clear();
            blackhole(box[0]);
        };
        long ns = measureWithSetup(setup, op);
        rows.add(row("BM_Clear", limbs, bench_iters, ns));
    }

    private static void benchReset(List<String[]> rows, int limbs) {
        final MutableBigInteger[] box = new MutableBigInteger[] {makeValue(limbs)};
        Runnable setup = () -> box[0] = makeValue(limbs);
        Runnable op = () -> {
            box[0].reset();
            blackhole(box[0]);
        };
        long ns = measureWithSetup(setup, op);
        rows.add(row("BM_Reset", limbs, bench_iters, ns));
    }

    private static long measure(Runnable op) {
        for (int w = 0; w < WARMUP_ROUNDS; w++) {
            for (int i = 0; i < bench_iters; i++) {
                op.run();
            }
        }
        long start = System.nanoTime();
        for (int i = 0; i < bench_iters; i++) {
            op.run();
        }
        long elapsed = System.nanoTime() - start;
        return elapsed / bench_iters;
    }

    private static long measureWithSetup(Runnable setup, Runnable op) {
        for (int w = 0; w < WARMUP_ROUNDS; w++) {
            for (int i = 0; i < bench_iters; i++) {
                setup.run();
                op.run();
            }
        }
        long start = System.nanoTime();
        for (int i = 0; i < bench_iters; i++) {
            setup.run();
            op.run();
        }
        long elapsed = System.nanoTime() - start;
        return elapsed / bench_iters;
    }

    private static MutableBigInteger makeValue(int limbs) {
        if (limbs <= 0) {
            return new MutableBigInteger();
        }
        if (limbs == 1) {
            return new MutableBigInteger(0x12345678);
        }
        return new MutableBigInteger(makeMag(limbs));
    }

    private static int[] makeMag(int limbs) {
        int[] mag = new int[limbs];
        for (int i = 0; i < limbs; i++) {
            mag[i] = 0x9e3779b9 + i * 0x85ebca6b;
        }
        return mag;
    }

    private static String[] row(String name, int limbs, int iters, long nsPerOp) {
        return new String[] {
            name,
            Integer.toString(limbs),
            Integer.toString(iters),
            Long.toString(nsPerOp)
        };
    }

    private static int blackhole_sink;

    private static void blackhole(MutableBigInteger n) {
        blackhole_sink += n.intLen;
    }
}
