# Wake-on-LAN 콘솔 애플리케이션

## 🌟 개요

**Wake-on-LAN (WOL)** 은 네트워크를 통해 원격으로 컴퓨터를 켜는 기술입니다. 이 프로그램은 Windows 명령 프롬프트에서 실행되는 간단한 WOL 도구로, 설정 파일에 지정된 대상 컴퓨터에 "매직 패킷"을 전송하여 원격으로 전원을 켤 수 있습니다.

### 🎯 주요 활용 사례
- 사무실에서 집 컴퓨터 원격 부팅
- 서버실의 장비 원격 관리

---

## ✨ 주요 기능

### 🔧 핵심 기능
- **매직 패킷 전송**: IEEE 802 표준에 따른 102바이트 WOL 패킷을 UDP 브로드캐스트로 전송
- **입력 검증**: MAC 주소, IP 주소, 포트 번호의 형식을 자동으로 검증하여 오류 방지
- **간편한 설정**: `config.ini` 파일 하나로 모든 설정 완료
- **다중 아키텍처 지원**: x64, x86, ARM64 플랫폼용 빌드 제공

### 🛡️ 안전성 특징
- 잘못된 설정값 입력 시 명확한 오류 메시지 제공
- 네트워크 연결 실패 시 상세한 진단 정보 출력
- 설정 파일 누락 시 친화적인 안내 메시지

---

## 📥 다운로드 및 설치

### 즉시 사용 (권장)
바로 사용할 수 있는 실행 파일을 다운로드하세요:

**👉 [최신 버전 다운로드](https://github.com/ohsungsik/Wake-On-Lan/releases/latest)**

### 설치 단계
1. 위 링크에서 본인의 시스템에 맞는 파일 다운로드
   - `WOL.x64.Release.exe` (64비트 Windows, 권장)
   - `WOL.x86.Release.exe` (32비트 Windows)
   - `WOL.ARM64.Release.exe` (ARM64 Windows)

2. 원하는 폴더에 실행 파일 저장

3. 같은 폴더에 `config.ini` 설정 파일 생성 (아래 설정 방법 참조)

---

## ⚙️ 설정 방법

### config.ini 파일 생성
실행 파일과 **반드시 같은 폴더**에 `config.ini` 파일을 만들고 다음 내용을 입력하세요:

```ini
[Target]
# 깨우려는 컴퓨터의 MAC 주소 (네트워크 어댑터 주소)
MacAddress=00-11-22-AA-BB-CC

# 브로드캐스트 IP 주소 (보통 192.168.x.255 형태)
BroadcastIp=192.168.0.255

# WOL 포트 번호 (기본값: 9)
Port=9
```

### 📝 설정값 찾는 방법

#### MAC 주소 확인 (대상 컴퓨터에서)
```cmd
# Windows 명령 프롬프트에서 실행
ipconfig /all
```
"물리적 주소" 또는 "Physical Address" 항목을 찾아 하이픈(`-`) 형식으로 입력

#### 브로드캐스트 IP 확인
- 일반적인 가정용 네트워크: `192.168.1.255` 또는 `192.168.0.255`
- 모든 네트워크에 전송: `255.255.255.255`
- 본인의 IP가 `192.168.1.100`이면 브로드캐스트는 `192.168.1.255`

#### 포트 설정
- 기본값 `9` 사용 권장
- 필요시 `7` 또는 다른 포트 사용 가능 (1~65535)

### ⚠️ 중요한 주의 사항
- `config.ini` 파일은 **UTF-8 인코딩**으로 저장해야 합니다
- 메모장에서 저장할 때 "인코딩: UTF-8" 선택
- MAC 주소는 반드시 하이픈(`-`) 구분자 사용

---

## 🚀 사용 방법

### 1단계: 명령 프롬프트 열기
- `Windows + R` → `cmd` 입력 → 엔터
- 또는 시작 메뉴에서 "명령 프롬프트" 검색

### 2단계: 프로그램 실행
```cmd
# 실행 파일이 있는 폴더로 이동
cd C:\다운로드\WOL

# 프로그램 실행
WOL.x64.Release.exe
```

### 3단계: 결과 확인
성공 시 다음과 같은 메시지가 표시됩니다:
```
매직 패킷이 성공적으로 전송되었습니다!
대상 PC가 켜지지 않는다면 다음을 확인하세요:
    1. 대상 PC의 BIOS에서 Wake-on-LAN 활성화
    2. 네트워크 어댑터의 전원 관리 설정
    3. 올바른 MAC 주소 및 브로드캐스트 IP
    4. 방화벽/라우터 설정
```

---

## 🔧 대상 컴퓨터 설정

Wake-on-LAN이 작동하려면 대상 컴퓨터에서 다음 설정이 필요합니다:

### BIOS/UEFI 설정

1) 컴퓨터 부팅 시 Delete, F2, F12 등을 눌러 BIOS 진입
2) "Power Management" 또는 "전원 관리" 메뉴 찾기
3) "Wake on LAN" 또는 "네트워크로 깨우기" 기능 활성화
4) 설정 저장 후 종료

### Windows 네트워크 어댑터 설정

#### 1단계: 전원 관리 설정

1) 장치 관리자 열기 (Windows + X → 장치 관리자)
2) 네트워크 어댑터 확장
3) 사용 중인 네트워크 어댑터 우클릭 → 속성
4) 전원 관리 탭 선택
5) 다음 옵션들 체크:
- 컴퓨터가 이 장치를 끌 수 있음
- 이 장치가 컴퓨터의 대기 모드를 해제할 수 있음
- 매직 패킷만 컴퓨터의 대기 모드를 해제할 수 있음

#### 2단계: 고급(Advanced) 속성 설정

1) 같은 네트워크 어댑터 속성 창에서 고급 탭 선택
2) 다음 항목들을 찾아서 설정 (제조사별로 이름이 다를 수 있음):

📌 필수 설정 항목들:
- Wake on Magic Packet: 사용 또는 Enabled
- Wake on Pattern Match: 사용 또는 Enabled
- WOL & Shutdown Link Speed: 10Mbps First 또는 Not Speed Down
- Energy Efficient Ethernet: 사용 안 함 또는 Disabled
- Green Ethernet: 사용 안 함 또는 Disabled
- 절전 모드에서 Magic Packet으로 깨우기: 사용

⚠️ 브랜드별 설정 이름 예시:
- Realtek: "Wake on Magic Packet", "WOL & Shutdown Link Speed"
- Intel: "Wake on Magic Packet", "Energy Efficient Ethernet"
- Broadcom: "Wake-Up Capabilities", "Energy Efficient Ethernet"

3) 설정 변경 후 확인 클릭
4) 컴퓨터 재부팅 (설정 적용을 위해)

## 🔍 설정 확인 방법
PowerCfg 명령어로 WOL 설정이 올바르게 되었는지 확인하는 방법입니다:

### 1단계: 관리자 권한 명령 프롬프트 열기

Windows + X 키 → "Windows PowerShell(관리자)" 또는 "명령 프롬프트(관리자)" 선택
또는 시작 메뉴에서 "cmd" 검색 → 우클릭 → "관리자 권한으로 실행"

### 2단계: 명령어 실행
```cmd
powercfg /devicequery wake_armed
```

### 3단계: 결과 해석

✅ 정상적인 경우 (WOL 설정 완료):
```
Intel(R) Ethernet Connection (2) I219-V
Realtek PCIe GbE Family Controller
HID-compliant mouse
USB Root Hub (3.0)
```
네트워크 어댑터 이름이 목록에 표시됩니다.

❌ 문제가 있는 경우:
```
HID-compliant mouse
USB Root Hub (3.0)
```
네트워크 어댑터가 목록에 없으면 WOL 설정이 제대로 되지 않은 것입니다.

---

## 🔨 직접 빌드하기

개발자이거나 소스코드를 수정하고 싶다면 직접 빌드 할 수 있습니다.

### 빌드 환경 요구사항
- **운영체제**: Windows 10 이상
- **개발도구**: Visual Studio 2019 이상 또는 Build Tools for Visual Studio
- **C++ 컴파일러**: MSVC (Microsoft Visual C++)

### 빌드 방법

#### 방법 1: 자동 빌드 스크립트
```cmd
# 프로젝트 폴더에서 실행
build.bat
```
빌드 완료 후 `out\bin\` 폴더에서 실행 파일 확인

#### 방법 2: Visual Studio 사용
1. `WOL.sln` 파일을 Visual Studio로 열기
2. 상단에서 플랫폼 선택 (x64/x86/ARM64)
3. 구성 선택 (Debug/Release)
4. **빌드** → **솔루션 빌드** (F7 또는 Ctrl+Shift+B)

---

## 📁 프로젝트 구조

```
WOL/                          # 프로젝트 루트 폴더
├── README.md                 # 이 문서
├── LICENSE                   # MIT 라이선스
├── .gitignore               # Git 버전 관리 제외 파일 목록
├── build.bat                # 자동 빌드 스크립트
├── main.cpp                 # C++ 소스 코드
├── WOL.sln                  # Visual Studio 솔루션 파일
├── WOL.vcxproj             # Visual Studio 프로젝트 파일
└── out/                     # 빌드 결과물 저장 폴더 (자동 생성)
    └── bin/                 # 실행 파일 저장 위치
        ├── WOL.x64.Release.exe
        ├── WOL.x86.Release.exe
        └── WOL.ARM64.Release.exe
```

---

## 📄 라이선스

이 프로젝트는 **MIT 라이선스** 하에 배포됩니다.

```
SPDX-License-Identifier: MIT
```

자유롭게 사용, 수정, 배포하실 수 있습니다.

---

## 🤝 기여 및 지원

### 버그 신고 및 기능 제안
GitHub Issues를 통해 버그 신고나 새로운 기능을 제안해 주세요:
- 🐛 **버그 신고**: 문제 상황과 재현 방법을 상세히 작성
- 💡 **기능 제안**: 필요한 기능과 사용 사례 설명
- 📚 **문서 개선**: 이해하기 어려운 부분이나 누락된 내용 지적

### 개발 참여
Pull Request를 통한 코드 기여를 환영합니다:
1. 프로젝트 Fork
2. 기능 브랜치 생성 (`git checkout -b feature/new-feature`)
3. 변경사항 커밋 (`git commit -am '새 기능 추가'`)
4. 브랜치에 Push (`git push origin feature/new-feature`)
5. Pull Request 생성

### 문의
- **이메일**: ohsungsik@outlook.com
- **GitHub Discussions**: 일반적인 질문과 토론
- **GitHub Issues**: 버그 신고 및 기능 요청
