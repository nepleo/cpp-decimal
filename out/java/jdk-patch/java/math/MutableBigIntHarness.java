package java.math;

import java.lang.reflect.Method;

/**
 * CLI harness for cross-checking C++ mutable_bigint against JDK {@link MutableBigInteger}.
 */
public final class MutableBigIntHarness {

    private MutableBigIntHarness() {}

    public static void main(String[] args) {
        if (args.length == 0) {
            System.err.println("usage: MutableBigIntHarness <command> [args...]");
            System.exit(2);
        }
        try {
            switch (args[0]) {
                case "ping" -> System.out.println("ok");
                case "default_ctor" -> System.out.println(state(new MutableBigInteger()));
                case "ctor_int" -> {
                    require(args, 2);
                    int val = Integer.parseInt(args[1]);
                    System.out.println(state(new MutableBigInteger(val)));
                }
                case "ctor_array" -> {
                    require(args, 2);
                    System.out.println(state(fromMagArg(args[1])));
                }
                case "clear_after_ctor" -> {
                    require(args, 2);
                    MutableBigInteger m = new MutableBigInteger(Integer.parseInt(args[1]));
                    m.clear();
                    System.out.println(state(m));
                }
                case "reset_after_ctor" -> {
                    require(args, 2);
                    MutableBigInteger m = new MutableBigInteger(Integer.parseInt(args[1]));
                    m.reset();
                    System.out.println(state(m));
                }
                case "copy_ctor" -> {
                    require(args, 2);
                    MutableBigInteger a = new MutableBigInteger(Integer.parseInt(args[1]));
                    System.out.println(state(new MutableBigInteger(a)));
                }
                case "state" -> {
                    require(args, 2);
                    System.out.println(state(fromMagArg(args[1])));
                }
                case "state_normalize" -> {
                    require(args, 2);
                    MutableBigInteger m = fromMagArg(args[1]);
                    m.normalize();
                    System.out.println(state(m));
                }
                case "limbs" -> {
                    require(args, 2);
                    System.out.println(limbs(fromMagArg(args[1])));
                }
                case "limbs_normalize" -> {
                    require(args, 2);
                    MutableBigInteger m = fromMagArg(args[1]);
                    m.normalize();
                    System.out.println(limbs(m));
                }
                case "flag" -> {
                    require(args, 3);
                    MutableBigInteger m = fromMagArg(args[2]);
                    boolean v = switch (args[1]) {
                        case "is_zero" -> m.isZero();
                        case "is_one" -> m.isOne();
                        case "is_even" -> m.isEven();
                        case "is_odd" -> m.isOdd();
                        case "is_normal" -> m.isNormal();
                        default -> throw new IllegalArgumentException("unknown flag: " + args[1]);
                    };
                    System.out.println("flag=" + v);
                }
                case "result_compare_int" -> {
                    require(args, 3);
                    MutableBigInteger a = new MutableBigInteger(Integer.parseInt(args[1]));
                    MutableBigInteger b = new MutableBigInteger(Integer.parseInt(args[2]));
                    System.out.println("result=" + a.compare(b));
                }
                case "result_compare_mag" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    System.out.println("result=" + a.compare(b));
                }
                case "result_compare_shifted" -> {
                    require(args, 4);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    int ints = Integer.parseInt(args[3]);
                    System.out.println("result=" + invokeCompareShifted(a, b, ints));
                }
                case "result_compare_half" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    System.out.println("result=" + a.compareHalf(b));
                }
                case "state_copy_value_int" -> {
                    require(args, 2);
                    MutableBigInteger dst = new MutableBigInteger();
                    dst.copyValue(new MutableBigInteger(Integer.parseInt(args[1])));
                    System.out.println(state(dst));
                }
                case "state_copy_value_mag" -> {
                    require(args, 2);
                    MutableBigInteger dst = new MutableBigInteger();
                    dst.copyValue(parseMag(args[1]));
                    System.out.println(state(dst));
                }
                case "state_copy_value_mbi" -> {
                    require(args, 2);
                    MutableBigInteger src = fromMagArg(args[1]);
                    MutableBigInteger dst = new MutableBigInteger();
                    dst.copyValue(src);
                    System.out.println(state(dst));
                }
                case "state_set_int" -> {
                    require(args, 4);
                    int base = Integer.parseInt(args[1]);
                    int index = Integer.parseInt(args[2]);
                    int limb = (int) Long.parseLong(args[3]);
                    MutableBigInteger m = new MutableBigInteger(base);
                    m.setInt(index, limb);
                    System.out.println(state(m));
                }
                case "state_set_value" -> {
                    require(args, 3);
                    int[] mag = parseMag(args[1]);
                    int len = Integer.parseInt(args[2]);
                    MutableBigInteger m = new MutableBigInteger();
                    m.setValue(mag, len);
                    System.out.println(state(m));
                }
                case "limbs_to_int_array" -> {
                    require(args, 2);
                    System.out.println(limbsUnsigned(fromMagArg(args[1]).toIntArray()));
                }
                case "state_get_magnitude" -> {
                    require(args, 2);
                    MutableBigInteger m = fromMagArg(args[1]);
                    m.normalize();
                    invokeGetMagnitudeArray(m);
                    System.out.println(state(m));
                }
                case "u64" -> {
                    require(args, 3);
                    MutableBigInteger m = fromMagArg(args[2]);
                    long v =
                            switch (args[1]) {
                                case "bit_length" -> invokeBitLength(m);
                                case "to_long" -> invokeToLong(m);
                                case "get_long" -> invokeGetLong(m, 0);
                                default ->
                                        throw new IllegalArgumentException(
                                                "unknown u64: " + args[1]);
                            };
                    System.out.println("u64=" + v);
                }
                case "i32" -> {
                    require(args, 3);
                    MutableBigInteger m = fromMagArg(args[2]);
                    int v =
                            switch (args[1]) {
                                case "lowest_set_bit" -> invokeLowestSetBit(m);
                                default ->
                                        throw new IllegalArgumentException(
                                                "unknown i32: " + args[1]);
                            };
                    System.out.println("result=" + v);
                }
                case "u32_at" -> {
                    require(args, 3);
                    MutableBigInteger m = fromMagArg(args[1]);
                    int index = Integer.parseInt(args[2]);
                    System.out.println(
                            "u32=" + Integer.toUnsignedString(invokeGetInt(m, index)));
                }
                case "u64_at" -> {
                    require(args, 3);
                    MutableBigInteger m = fromMagArg(args[1]);
                    int index = Integer.parseInt(args[2]);
                    System.out.println("u64=" + invokeGetLong(m, index));
                }
                case "state_shift" -> {
                    require(args, 4);
                    MutableBigInteger m = fromMagArg(args[2]);
                    int n = Integer.parseInt(args[3]);
                    applyShift(args[1], m, n);
                    System.out.println(state(m));
                }
                case "state_add" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    a.add(b);
                    System.out.println(state(a));
                }
                case "state_add_set" -> {
                    require(args, 4);
                    int[] mag = parseMag(args[1]);
                    int len = Integer.parseInt(args[2]);
                    MutableBigInteger a = new MutableBigInteger();
                    a.setValue(mag, len);
                    MutableBigInteger b = fromMagArg(args[3]);
                    a.add(b);
                    System.out.println(state(a));
                }
                case "result_subtract" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    System.out.println("result=" + a.subtract(b));
                }
                case "state_subtract" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    a.subtract(b);
                    System.out.println(state(a));
                }
                case "state_subtract_set" -> {
                    require(args, 4);
                    int[] mag = parseMag(args[1]);
                    int len = Integer.parseInt(args[2]);
                    MutableBigInteger a = new MutableBigInteger();
                    a.setValue(mag, len);
                    MutableBigInteger b = fromMagArg(args[3]);
                    a.subtract(b);
                    System.out.println(state(a));
                }
                case "result_difference" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    System.out.println("result=" + invokeDifference(a, b));
                }
                case "state_difference_winner" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    int sign = invokeDifference(a, b);
                    System.out.println(state(sign < 0 ? b : a));
                }
                case "state_add_disjoint" -> {
                    require(args, 4);
                    MutableBigInteger a = fromMagArg(args[1]);
                    int n = Integer.parseInt(args[2]);
                    MutableBigInteger addend = fromMagArg(args[3]);
                    a.addDisjoint(addend, n);
                    System.out.println(state(a));
                }
                case "state_add_shifted" -> {
                    require(args, 4);
                    MutableBigInteger a = fromMagArg(args[1]);
                    int n = Integer.parseInt(args[2]);
                    MutableBigInteger addend = fromMagArg(args[3]);
                    a.addShifted(addend, n);
                    System.out.println(state(a));
                }
                case "state_add_lower" -> {
                    require(args, 4);
                    MutableBigInteger a = fromMagArg(args[1]);
                    int n = Integer.parseInt(args[2]);
                    MutableBigInteger addend = fromMagArg(args[3]);
                    a.addLower(addend, n);
                    System.out.println(state(a));
                }
                case "state_ensure_capacity" -> {
                    require(args, 3);
                    MutableBigInteger m = fromMagArg(args[1]);
                    int len = Integer.parseInt(args[2]);
                    invokeEnsureCapacity(m, len);
                    System.out.println(state(m));
                }
                case "i32_ntz" -> {
                    require(args, 2);
                    int limb = (int) Long.parseLong(args[1]);
                    System.out.println("result=" + Integer.numberOfTrailingZeros(limb));
                }
                case "i32_nlz" -> {
                    require(args, 2);
                    int limb = (int) Long.parseLong(args[1]);
                    System.out.println("result=" + Integer.numberOfLeadingZeros(limb));
                }
                case "state_one" -> System.out.println(state(MutableBigInteger.ONE));
                case "state_mul" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    int y = (int) Long.parseLong(args[2]);
                    MutableBigInteger z = new MutableBigInteger();
                    a.mul(y, z);
                    System.out.println(state(z));
                }
                case "state_multiply" -> {
                    require(args, 3);
                    MutableBigInteger x = fromMagArg(args[1]);
                    MutableBigInteger y = fromMagArg(args[2]);
                    MutableBigInteger z = new MutableBigInteger();
                    x.multiply(y, z);
                    System.out.println(state(z));
                }
                case "state_divide_quotient" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    MutableBigInteger q = new MutableBigInteger();
                    a.divide(b, q);
                    q.normalize();
                    System.out.println(state(q));
                }
                case "state_divide_remainder" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    MutableBigInteger q = new MutableBigInteger();
                    MutableBigInteger r = a.divide(b, q);
                    r.normalize();
                    System.out.println(state(r));
                }
                case "limbs_divide_quotient" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    MutableBigInteger q = new MutableBigInteger();
                    a.divide(b, q);
                    q.normalize();
                    System.out.println(limbs(q));
                }
                case "limbs_divide_remainder" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    MutableBigInteger q = new MutableBigInteger();
                    MutableBigInteger r = a.divide(b, q);
                    r.normalize();
                    System.out.println(limbs(r));
                }
                case "u64_divide" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    long v = Long.parseUnsignedLong(args[2]);
                    MutableBigInteger q = new MutableBigInteger();
                    long rem = a.divide(v, q);
                    System.out.println("u64=" + rem);
                }
                case "state_divide_quotient_u64" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    long v = Long.parseUnsignedLong(args[2]);
                    MutableBigInteger q = new MutableBigInteger();
                    a.divide(v, q);
                    System.out.println(state(q));
                }
                case "state_divide_one_word_quotient" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    int divisor = (int) Long.parseUnsignedLong(args[2]);
                    MutableBigInteger q = new MutableBigInteger();
                    invokeDivideOneWord(a, divisor, q);
                    System.out.println(state(q));
                }
                case "u32_divide_one_word" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    int divisor = (int) Long.parseUnsignedLong(args[2]);
                    MutableBigInteger q = new MutableBigInteger();
                    int rem = invokeDivideOneWord(a, divisor, q);
                    System.out.println("u32=" + Integer.toUnsignedString(rem));
                }
                case "state_sqrt" -> {
                    require(args, 2);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger r = a.sqrt();
                    r.normalize();
                    System.out.println(state(r));
                }
                case "u32_binary_gcd" -> {
                    require(args, 3);
                    int a = (int) Long.parseUnsignedLong(args[1]);
                    int b = (int) Long.parseUnsignedLong(args[2]);
                    System.out.println("u32=" + Integer.toUnsignedString(MutableBigInteger.binaryGcd(a, b)));
                }
                case "state_hybrid_gcd" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    System.out.println(state(a.hybridGCD(b)));
                }
                case "state_binary_gcd_mag" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger b = fromMagArg(args[2]);
                    System.out.println(state(invokeBinaryGcd(a, b)));
                }
                case "u32_inverse_mod32" -> {
                    require(args, 2);
                    int val = (int) Long.parseUnsignedLong(args[1]);
                    System.out.println("u32=" + Integer.toUnsignedString(MutableBigInteger.inverseMod32(val)));
                }
                case "u64_inverse_mod64" -> {
                    require(args, 2);
                    long val = Long.parseUnsignedLong(args[1]);
                    System.out.println("u64=" + Long.toUnsignedString(MutableBigInteger.inverseMod64(val)));
                }
                case "state_mod_inverse_mp2" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    int k = Integer.parseInt(args[2]);
                    System.out.println(state(a.modInverseMP2(k)));
                }
                case "state_mod_inverse" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger mod = fromMagArg(args[2]);
                    System.out.println(state(invokeModInverse(a, mod)));
                }
                case "state_mutable_mod_inverse" -> {
                    require(args, 3);
                    MutableBigInteger a = fromMagArg(args[1]);
                    MutableBigInteger mod = fromMagArg(args[2]);
                    System.out.println(state(a.mutableModInverse(mod)));
                }
                case "state_mod_inverse_bp2" -> {
                    require(args, 3);
                    MutableBigInteger mod = fromMagArg(args[1]);
                    int k = Integer.parseInt(args[2]);
                    System.out.println(state(MutableBigInteger.modInverseBP2(mod, k)));
                }
                case "state_shift_set" -> {
                    require(args, 5);
                    int[] mag = parseMag(args[2]);
                    int len = Integer.parseInt(args[3]);
                    int n = Integer.parseInt(args[4]);
                    MutableBigInteger m = new MutableBigInteger();
                    m.setValue(mag, len);
                    applyShift(args[1], m, n);
                    System.out.println(state(m));
                }
                default -> {
                    System.err.println("unknown command: " + args[0]);
                    System.exit(1);
                }
            }
        } catch (Exception e) {
            System.err.println(e.getMessage());
            System.exit(1);
        }
    }

    private static void require(String[] args, int n) {
        if (args.length < n) {
            throw new IllegalArgumentException("missing arguments for " + args[0]);
        }
    }

    private static MutableBigInteger fromMagArg(String csv) {
        if ("empty".equals(csv)) {
            return new MutableBigInteger();
        }
        int[] mag = parseMag(csv);
        if (mag.length == 0) {
            return new MutableBigInteger();
        }
        return new MutableBigInteger(mag);
    }

    private static int[] parseMag(String csv) {
        if (csv.isEmpty()) {
            return new int[0];
        }
        String[] parts = csv.split(",");
        int[] mag = new int[parts.length];
        for (int i = 0; i < parts.length; i++) {
            mag[i] = (int) Long.parseLong(parts[i].trim());
        }
        return mag;
    }

    static String state(MutableBigInteger m) {
        StringBuilder sb = new StringBuilder();
        sb.append("int_len=").append(m.intLen);
        sb.append(" offset=").append(m.offset);
        sb.append(" buf_len=").append(m.value.length);
        sb.append(" limbs=");
        for (int i = 0; i < m.intLen; i++) {
            if (i > 0) {
                sb.append(',');
            }
            sb.append(Integer.toUnsignedString(m.value[m.offset + i]));
        }
        return sb.toString();
    }

    static String limbs(MutableBigInteger m) {
        return limbsUnsigned(m.toIntArray());
    }

    private static int invokeCompareShifted(MutableBigInteger a, MutableBigInteger b, int ints)
            throws Exception {
        Method m =
                MutableBigInteger.class.getDeclaredMethod(
                        "compareShifted", MutableBigInteger.class, int.class);
        m.setAccessible(true);
        return (int) m.invoke(a, b, ints);
    }

    private static void invokeGetMagnitudeArray(MutableBigInteger m) throws Exception {
        Method method = MutableBigInteger.class.getDeclaredMethod("getMagnitudeArray");
        method.setAccessible(true);
        method.invoke(m);
    }

    private static long invokeBitLength(MutableBigInteger m) throws Exception {
        Method method = MutableBigInteger.class.getDeclaredMethod("bitLength");
        method.setAccessible(true);
        return (long) method.invoke(m);
    }

    private static int invokeLowestSetBit(MutableBigInteger m) throws Exception {
        Method method = MutableBigInteger.class.getDeclaredMethod("getLowestSetBit");
        method.setAccessible(true);
        return (int) method.invoke(m);
    }

    private static long invokeToLong(MutableBigInteger m) throws Exception {
        Method method = MutableBigInteger.class.getDeclaredMethod("toLong");
        method.setAccessible(true);
        return (long) method.invoke(m);
    }

    private static long invokeGetLong(MutableBigInteger m, int index) throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod("getLong", int.class);
        method.setAccessible(true);
        return (long) method.invoke(m, index);
    }

    private static int invokeGetInt(MutableBigInteger m, int index) throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod("getInt", int.class);
        method.setAccessible(true);
        return (int) method.invoke(m, index);
    }

    private static void applyShift(String op, MutableBigInteger m, int n) throws Exception {
        switch (op) {
            case "primitive_right_shift" -> invokePrimitiveRightShift(m, n);
            case "primitive_left_shift" -> invokePrimitiveLeftShift(m, n);
            case "right_shift" -> m.rightShift(n);
            case "left_shift" -> m.leftShift(n);
            case "safe_right_shift" -> m.safeRightShift(n);
            case "safe_left_shift" -> m.safeLeftShift(n);
            default -> throw new IllegalArgumentException("unknown shift: " + op);
        }
    }

    private static void invokePrimitiveRightShift(MutableBigInteger m, int n)
            throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod("primitiveRightShift", int.class);
        method.setAccessible(true);
        method.invoke(m, n);
    }

    private static void invokePrimitiveLeftShift(MutableBigInteger m, int n) throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod("primitiveLeftShift", int.class);
        method.setAccessible(true);
        method.invoke(m, n);
    }

    private static int invokeDifference(MutableBigInteger a, MutableBigInteger b) throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod(
                        "difference", MutableBigInteger.class);
        method.setAccessible(true);
        return (int) method.invoke(a, b);
    }

    private static void invokeEnsureCapacity(MutableBigInteger m, int len) throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod("ensureCapacity", int.class);
        method.setAccessible(true);
        method.invoke(m, len);
    }

    private static int invokeDivideOneWord(
            MutableBigInteger a, int divisor, MutableBigInteger quotient) throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod(
                        "divideOneWord", int.class, MutableBigInteger.class);
        method.setAccessible(true);
        return (int) method.invoke(a, divisor, quotient);
    }

    private static MutableBigInteger invokeBinaryGcd(MutableBigInteger a, MutableBigInteger b)
            throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod(
                        "binaryGCD", MutableBigInteger.class);
        method.setAccessible(true);
        return (MutableBigInteger) method.invoke(a, b);
    }

    private static MutableBigInteger invokeModInverse(MutableBigInteger a, MutableBigInteger mod)
            throws Exception {
        Method method =
                MutableBigInteger.class.getDeclaredMethod(
                        "modInverse", MutableBigInteger.class);
        method.setAccessible(true);
        return (MutableBigInteger) method.invoke(a, mod);
    }

    static String limbsUnsigned(int[] mag) {
        StringBuilder sb = new StringBuilder("limbs=");
        for (int i = 0; i < mag.length; i++) {
            if (i > 0) {
                sb.append(',');
            }
            sb.append(Integer.toUnsignedString(mag[i]));
        }
        return sb.toString();
    }
}
