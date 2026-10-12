package iris.core;

/* Native declarations for the Android feasibility bridge. This is not an application screen. */
public final class IrisNative {
    static {
        System.loadLibrary("iris_jni");
    }

    private IrisNative() {}

    public static native long create();

    public static native void destroy(long handle);

    public static native int load(long handle, byte[] bytes, String name);

    public static native void runFrame(long handle);

    public static native byte[] frame(long handle);

    public static native long framePeriodNs(long handle);
}
