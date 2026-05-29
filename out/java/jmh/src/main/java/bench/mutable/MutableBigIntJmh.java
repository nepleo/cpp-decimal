package bench.mutable;

import java.math.MutableBigIntBenchAccess;
import java.util.concurrent.TimeUnit;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Level;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.infra.Blackhole;

@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
public class MutableBigIntJmh {

    @Benchmark
    public void BM_DefaultCtor(Blackhole bh) {
        bh.consume(MutableBigIntBenchAccess.newDefault());
    }

    @Benchmark
    public void BM_UintCtor(Blackhole bh) {
        bh.consume(MutableBigIntBenchAccess.newFromInt(0xdeadbeef));
    }

    @State(Scope.Benchmark)
    public static class JarrayCtorState {
        @Param({"1", "4", "16", "64"})
        public int limbs;

        public int[] mag;

        @Setup(Level.Trial)
        public void initMag() {
            mag = MutableBigIntBenchAccess.makeMag(limbs);
        }
    }

    @Benchmark
    public void BM_JarrayCtor(JarrayCtorState s, Blackhole bh) {
        bh.consume(MutableBigIntBenchAccess.newFromMagCopy(s.mag));
    }

    @State(Scope.Benchmark)
    public static class CopyState {
        @Param({"1", "4", "16", "64"})
        public int limbs;

        public Object src;

        @Setup(Level.Trial)
        public void initSrc() {
            src = MutableBigIntBenchAccess.makeValue(limbs);
        }
    }

    @Benchmark
    public void BM_CopyCtor(CopyState s, Blackhole bh) {
        bh.consume(MutableBigIntBenchAccess.newCopy(s.src));
    }

    @State(Scope.Benchmark)
    public static class CopyAssignState {
        @Param({"1", "4", "16", "64"})
        public int limbs;

        public Object src;
        public Object dst;

        @Setup(Level.Trial)
        public void init() {
            src = MutableBigIntBenchAccess.makeValue(limbs);
            dst = MutableBigIntBenchAccess.newEmpty();
        }
    }

    @Benchmark
    public void BM_CopyAssign(CopyAssignState s, Blackhole bh) {
        MutableBigIntBenchAccess.copyAssign(s.dst, s.src);
        bh.consume(s.dst);
    }

    @Benchmark
    public void BM_MoveCtor(CopyState s, Blackhole bh) {
        Object src = MutableBigIntBenchAccess.makeValue(s.limbs);
        Object dst = MutableBigIntBenchAccess.newCopy(src);
        bh.consume(dst);
        bh.consume(src);
    }

    @State(Scope.Benchmark)
    public static class MoveAssignState {
        @Param({"1", "4", "16", "64"})
        public int limbs;
    }

    @Benchmark
    public void BM_MoveAssign(MoveAssignState s, Blackhole bh) {
        Object src = MutableBigIntBenchAccess.makeValue(s.limbs);
        Object dst = MutableBigIntBenchAccess.newEmpty();
        MutableBigIntBenchAccess.copyAssign(dst, src);
        bh.consume(dst);
        bh.consume(src);
    }

    @State(Scope.Thread)
    public static class ClearResetState {
        @Param({"1", "16", "64"})
        public int limbs;

        public Object n;

        @Setup(Level.Iteration)
        public void restoreNonZero() {
            n = MutableBigIntBenchAccess.makeValue(limbs);
        }
    }

    @Benchmark
    public void BM_Clear(ClearResetState s, Blackhole bh) {
        MutableBigIntBenchAccess.clear(s.n);
        bh.consume(MutableBigIntBenchAccess.intLen(s.n));
    }

    @Benchmark
    public void BM_Reset(ClearResetState s, Blackhole bh) {
        MutableBigIntBenchAccess.reset(s.n);
        bh.consume(MutableBigIntBenchAccess.intLen(s.n));
    }

    @State(Scope.Benchmark)
    public static class MulState {
        @Param({"1", "4", "16", "64"})
        public int limbs;

        public Object x;
        public Object z;

        @Setup(Level.Trial)
        public void init() {
            x = MutableBigIntBenchAccess.makeValue(limbs);
            z = MutableBigIntBenchAccess.newEmpty();
        }
    }

    @Benchmark
    public void BM_Mul(MulState s, Blackhole bh) {
        MutableBigIntBenchAccess.mul(s.x, 0x9e3779b9, s.z);
        bh.consume(MutableBigIntBenchAccess.intLen(s.z));
    }

    @State(Scope.Benchmark)
    public static class MultiplyState {
        @Param({"1", "4", "16", "64"})
        public int limbs;

        public Object x;
        public Object y;
        public Object z;

        @Setup(Level.Trial)
        public void init() {
            x = MutableBigIntBenchAccess.makeValue(limbs);
            y = MutableBigIntBenchAccess.makeValue(limbs);
            z = MutableBigIntBenchAccess.newEmpty();
        }
    }

    @Benchmark
    public void BM_Multiply(MultiplyState s, Blackhole bh) {
        MutableBigIntBenchAccess.multiply(s.x, s.y, s.z);
        bh.consume(MutableBigIntBenchAccess.intLen(s.z));
    }

    @State(Scope.Benchmark)
    public static class DivideState {
        @Param({"1", "4", "16", "64"})
        public int limbs;

        public Object x;
        public Object y;
        public Object q;

        @Setup(Level.Trial)
        public void init() {
            x = MutableBigIntBenchAccess.makeValue(limbs);
            y = MutableBigIntBenchAccess.makeValue(Math.max(1, limbs / 2));
            q = MutableBigIntBenchAccess.newEmpty();
        }
    }

    @Benchmark
    public void BM_Divide(DivideState s, Blackhole bh) {
        Object r = MutableBigIntBenchAccess.divide(s.x, s.y, s.q);
        bh.consume(r);
        bh.consume(MutableBigIntBenchAccess.intLen(s.q));
    }

    @State(Scope.Benchmark)
    public static class SqrtState {
        @Param({"1", "4", "16", "64"})
        public int limbs;

        public Object x;

        @Setup(Level.Trial)
        public void init() {
            x = MutableBigIntBenchAccess.makeValue(limbs);
        }
    }

    @Benchmark
    public void BM_Sqrt(SqrtState s, Blackhole bh) {
        bh.consume(MutableBigIntBenchAccess.sqrt(s.x));
    }

    @State(Scope.Benchmark)
    public static class GcdState {
        @Param({"1", "4", "16", "64"})
        public int limbs;

        public Object x;
        public Object y;

        @Setup(Level.Trial)
        public void init() {
            x = MutableBigIntBenchAccess.makeValue(limbs);
            y = MutableBigIntBenchAccess.makeValue(limbs);
        }
    }

    @Benchmark
    public void BM_HybridGcd(GcdState s, Blackhole bh) {
        bh.consume(MutableBigIntBenchAccess.hybridGcd(s.x, s.y));
    }

    @State(Scope.Benchmark)
    public static class ModInverseState {
        public Object x;
        public Object mod;

        @Setup(Level.Trial)
        public void init() throws Exception {
            x = MutableBigIntBenchAccess.newFromInt(3);
            mod = MutableBigIntBenchAccess.newFromInt(65537);
        }
    }

    @Benchmark
    public void BM_ModInverse(ModInverseState s, Blackhole bh) throws Exception {
        bh.consume(MutableBigIntBenchAccess.modInverse(s.x, s.mod));
    }
}
