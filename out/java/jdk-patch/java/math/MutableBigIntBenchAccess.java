package java.math;

import java.util.Arrays;

/**
 * Public bridge for JMH benchmarks (MutableBigInteger is package-private in java.base).
 * Mirrors scenarios in C++ {@code benchmark/bench_mutable_bigint.cpp}.
 */
public final class MutableBigIntBenchAccess {

    private MutableBigIntBenchAccess() {}

    public static Object newDefault() {
        return new MutableBigInteger();
    }

    public static Object newFromInt(int val) {
        return new MutableBigInteger(val);
    }

    /** Copy magnitude array (fair vs C++ jarray copy ctor). */
    public static Object newFromMagCopy(int[] mag) {
        return new MutableBigInteger(Arrays.copyOf(mag, mag.length));
    }

    public static Object newCopy(Object src) {
        return new MutableBigInteger((MutableBigInteger) src);
    }

    public static Object newEmpty() {
        return new MutableBigInteger();
    }

    public static void copyAssign(Object dst, Object src) {
        ((MutableBigInteger) dst).copyValue((MutableBigInteger) src);
    }

    public static void clear(Object n) {
        ((MutableBigInteger) n).clear();
    }

    public static void reset(Object n) {
        ((MutableBigInteger) n).reset();
    }

    /** Read package-private field so JMH cannot eliminate the operation. */
    public static int intLen(Object n) {
        return ((MutableBigInteger) n).intLen;
    }

    public static Object makeValue(int limbs) {
        if (limbs <= 0) {
            return new MutableBigInteger();
        }
        if (limbs == 1) {
            return new MutableBigInteger(0x12345678);
        }
        int[] mag = new int[limbs];
        for (int i = 0; i < limbs; i++) {
            mag[i] = 0x9e3779b9 + i * 0x85ebca6b;
        }
        return new MutableBigInteger(mag);
    }

    public static int[] makeMag(int limbs) {
        int[] mag = new int[limbs];
        for (int i = 0; i < limbs; i++) {
            mag[i] = 0x9e3779b9 + i * 0x85ebca6b;
        }
        return mag;
    }

    public static void mul(Object x, int y, Object z) {
        ((MutableBigInteger) x).mul(y, (MutableBigInteger) z);
    }

    public static void multiply(Object x, Object y, Object z) {
        ((MutableBigInteger) x).multiply((MutableBigInteger) y, (MutableBigInteger) z);
    }

    public static Object divide(Object x, Object y, Object q) {
        return ((MutableBigInteger) x).divide((MutableBigInteger) y, (MutableBigInteger) q);
    }

    public static Object sqrt(Object x) {
        return ((MutableBigInteger) x).sqrt();
    }

    public static Object hybridGcd(Object x, Object y) {
        return ((MutableBigInteger) x).hybridGCD((MutableBigInteger) y);
    }

    public static Object modInverse(Object x, Object mod) throws Exception {
        java.lang.reflect.Method m =
                MutableBigInteger.class.getDeclaredMethod(
                        "modInverse", MutableBigInteger.class);
        m.setAccessible(true);
        return m.invoke(x, mod);
    }
}
