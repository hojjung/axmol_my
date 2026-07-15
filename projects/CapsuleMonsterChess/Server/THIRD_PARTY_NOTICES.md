# Third-party notices

서버 독립 빌드가 ExaStudio checkout에서 직접 참조하는 외부 소프트웨어다. 배포 패키지에는 각 원본
라이선스 전문을 함께 포함한다.

| 소프트웨어 | 사용 범위 | 버전/라이선스 | 로컬 원본 |
|---|---|---|---|
| Boost | header-only Asio, Beast | 1.90.0, Boost Software License 1.0 | `ExaStudio/exaEngine/3rd_party/boost`, `LICENSE_1_0.txt` |
| JSON for Modern C++ | `Util/json.hpp` 단일 헤더 | 3.12.0, MIT | `ExaStudio/exaEngine/core/exaCore/include/Util/json.hpp`, `3rd_party/llama/licenses/LICENSE-jsonhpp` |
| Mbed TLS | `sha256.c`와 필요한 zeroize shim만 컴파일 | 3.6.6, Apache-2.0 선택 | `ExaStudio/exaEngine/3rd_party/mbedtls`, `LICENSE` |
| Axmol Engine | Scheduler, Event, Object 등 선택 core 소스 | 저장소 현재 revision, MIT | 저장소 루트 `LICENSE` |
| {fmt} | Axmol core 헤더 의존 | 12.2.0, MIT | `3rdparty/fmt/include/fmt/format.h` |
| robin-map | Axmol listener map 헤더 의존 | 1.3.0, MIT | `3rdparty/robin-map/include/tsl/robin_map.h` |
| yasio | Axmol platform/string-view 헤더 의존 | 4.4.0, MIT | `3rdparty/yasio/yasio/yasio.hpp` |

Axmol과 Capsule Monster Chess 자체 라이선스는 저장소 루트 정책을 따른다.
