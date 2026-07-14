# Platform adapters

상위 클라이언트는 `IIdentityService`와 `IBackendClient`만 사용한다.

- WebAssembly: Firebase modular JavaScript SDK가 Auth, App Check, callable을 처리하고 얇은
  C ABI/`EM_JS` bridge로 C++에 결과만 전달한다.
- Android/iOS: Firebase C++ SDK adapter를 사용한다.
- macOS/Windows 개발 빌드: native adapter 또는 명시적인 local dummy adapter를 사용한다.

Firebase SDK, JS 값, platform token은 `GameCore`로 전달하지 않는다.
