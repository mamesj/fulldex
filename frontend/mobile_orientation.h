#pragma once
#include <SDL.h>
#ifdef __ANDROID__
#include <jni.h>
#endif
#ifdef SSR_IOS
#include "ios_orient.h"
#endif

namespace MobileOrientation
{
// The explicit stacked DS layout uses an upright phone; other game layouts stay sideways.
inline void Game(bool portrait)
{
#if defined(__ANDROID__) || defined(SSR_IOS)
    SDL_SetHint(SDL_HINT_ORIENTATIONS, portrait ? "Portrait PortraitUpsideDown" : "LandscapeLeft LandscapeRight");
#ifdef __ANDROID__
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    auto activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return;
    jclass cls = env->GetObjectClass(activity);
    if (cls)
    {
        jmethodID method = env->GetStaticMethodID(cls, portrait ? "showPortraitGameOrientation" : "showGameOrientation", "()V");
        if (method) env->CallStaticVoidMethod(cls, method);
        // A standalone SDL Activity uses the hint when creating its window, without this bridge.
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
    }
    env->DeleteLocalRef(activity);
#else
    IosOrientGame(portrait);
#endif
#else
    (void)portrait;
#endif
}
}
