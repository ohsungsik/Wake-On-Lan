////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Wake-on-LAN(WOL) 기능을 제공하는 콘솔 애플리케이션
/// 
/// @details
/// 이 프로그램은 config.ini 설정 파일로부터 MAC 주소, 브로드캐스트 IP, 포트 정보를 읽어
/// 대상 PC로 WOL(Wake-on-LAN) 매직 패킷을 전송합니다.
///
/// - INI 파일 위치: 실행 파일과 동일한 디렉토리
/// - INI 인코딩: UTF-8 (BOM 없이 저장)
/// - INI 파일 구조:
///   [Target]
///   MacAddress=00-11-22-AA-BB-CC
///   BroadcastIp=192.168.0.255
///   Port=9
///
/// - 테스트 환경: Windows 10 이상
/// - 유의 사항:
///   - 대상 장치의 BIOS/UEFI에서 WOL 기능이 활성화되어 있어야 함
///   - 네트워크 장치 및 방화벽 설정이 WOL 패킷을 허용해야 함
///
/// @author Oh Sungsik <ohsungsik@outlook.com>
/// @version 1.0
/// @date 2025-05-30
///
/// @license
/// This code is released under the MIT License.
/// You are free to use, modify, and distribute it with attribution.
///
/// SPDX-License-Identifier: MIT
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cassert>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <io.h>
#include <sal.h>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <MSWSock.h>	// WinSock2.h 헤더 하위에 있어야 함


#pragma comment(lib, "ws2_32.lib")

/// @brief 설정 파일명 상수
/// @details 실행 파일과 동일한 디렉토리에 위치하는 설정 파일의 고정 이름
///          변경 불가능한 컴파일 타임 상수로 정의
#define CONFIG_FILE_NAME L"config.ini"

namespace WakeOnLan
{
    /// @brief MAC 주소를 저장하는 타입 (6바이트 고정 크기 배열)
    /// @details IEEE 802 표준에 따른 6바이트 하드웨어 주소를 저장
    ///          네트워크 인터페이스 카드의 고유 물리적 주소를 나타냄
    using MacAddress = std::array<std::byte, 6U>;

    /// @brief Wake-on-LAN 매직 패킷을 저장하는 타입 (총 102바이트)
    /// @details WOL 표준에 따른 매직 패킷 구조:
    ///          - 6바이트 동기화 헤더 (0xFF 6개)
    ///          - 16회 반복되는 대상 MAC 주소 (6바이트 × 16 = 96바이트)
    ///          - 총 102바이트로 구성된 Wake-on-LAN 패킷
    using MagicPacket = std::array<std::byte, 102U>;

    ///	@brief Wake-on-LAN 패킷 전송 결과를 나타내는 열거형
    /// @details 각 단계에서 발생할 수 있는 오류 상황을 구분하여 정의
    ///          디버깅 및 오류 처리 시 정확한 원인 파악을 위해 세분화됨
    enum class WolErrorCode : std::uint8_t
    {
        // 성공
        Success = 0U, /// 성공

        // Config 파일 관련
        // Config 파일을 찾는 과정에서 발생하는 오류
        FailedToGetExecutionPath, /// 실행 파일 경로를 얻을 수 없음
        InvalidExecutionPath, /// 유효하지 않은 실행 파일 경로

        // Config 파일을 읽는 과정에서 발생하는 오류
        ConfigFileNotFound, /// Config 파일을 찾을 수 없음
        CannotAccessConfigFile, /// Config 파일에 접근 권한이 없음
        FailedToReadMacAddress, /// Config 파일에서 Mac 주소를 읽을 수 없음
        InvalidMacAddress, /// 유효하지 않은 MAC 주소
        FailedToReadBroadcastIp, /// Config 파일에서 브로드캐스트 주소를 읽을 수 없음
        InvalidBroadcastIp, /// 유효하지 않은 브로드캐스트 주소
        FailedToReadPort, /// Config 파일에서 포트 읽을 수 없음
        InvalidPort, /// 유효하지 않은 포트

        // WOL 매직 패킷을 보내는 과정에서 발생하는 오류
        WinsockInitializationFailed, /// Winsock 라이브러리 초기화 실패
        SocketCreationFailed, /// UDP 소켓 생성 실패
        BroadcastSetupFailed, /// 브로드캐스트 소켓 옵션 설정 실패
        PacketSendFailed, /// 패킷 전송 과정에서 네트워크 오류 발생

        // 기타
        UnexpectedException /// 예상치 못한 예외 상황
    };

    namespace
    {
        /// @brief WOL 결과를 문자열로 변환
        ///	@param result WOL 결과
        ///	@return 결과 설명 문자열
        [[nodiscard]] std::wstring WolErrorCodeToString(_In_ const WolErrorCode result) noexcept
        {
            switch (result)
            {
            case WolErrorCode::Success: return {L"성공\n"};

            case WolErrorCode::FailedToGetExecutionPath: return {L"실행 파일 경로를 얻을 수 없음\n"};
            case WolErrorCode::InvalidExecutionPath: return {L"유효하지 않은 실행 파일 경로\n"};

            case WolErrorCode::ConfigFileNotFound: return {CONFIG_FILE_NAME L"파일을 찾을 수 없음\n"};
            case WolErrorCode::CannotAccessConfigFile: return {CONFIG_FILE_NAME L"파일에 접근 권한이 없음\n"};
            case WolErrorCode::FailedToReadMacAddress: return {L"Config 파일에서 Mac 주소를 읽을 수 없음\n"};
            case WolErrorCode::InvalidMacAddress: return {L"잘못된 MAC 주소\n"};
            case WolErrorCode::FailedToReadBroadcastIp: return {L"Config 파일에서 브로드캐스트 주소를 읽을 수 없음\n"};
            case WolErrorCode::InvalidBroadcastIp: return {L"잘못된 브로드캐스트 주소\n"};
            case WolErrorCode::FailedToReadPort: return {L"Config 파일에서 포트 읽을 수 없음\n"};
            case WolErrorCode::InvalidPort: return {L"잘못된 포트 번호\n"};

            case WolErrorCode::WinsockInitializationFailed: return {L"WinSock 초기화 실패\n"};
            case WolErrorCode::SocketCreationFailed: return {L"소켓 생성 실패\n"};
            case WolErrorCode::BroadcastSetupFailed: return {L"브로드캐스트 설정 실패\n"};
            case WolErrorCode::PacketSendFailed: return {L"패킷 전송 실패\n"};

            case WolErrorCode::UnexpectedException: return {L"예기치 않은 오류 발생\n"};
            }

            // 모든 조건을 위 Switch 문에서 처리해야 함
            assert(false);
            return {L""};
        }
    }

    /// @brief Wake-on-LAN 기능을 위한 설정 구조체
    /// @details WOL 대상 장치의 설정 정보를 관리하는 클래스
    ///          - 대상 장치의 MAC 주소 저장 및 관리
    ///          - 브로드캐스트 IP 주소 및 포트 번호 설정
    ///          - INI 파일을 통한 설정 로드 기능 제공
    struct WolConfig final
    {
    public:
        /// @brief 기본 생성자
        /// @details 모든 멤버 변수를 기본값으로 초기화
        ///          mMacAddress와 mBroadcastIP는 빈 문자열로 초기화
        ///          mPort는 유효하지 않은 포트 번호(0)으로 초기화
        WolConfig() = default;

        /// @brief 복사 생성자 - 사용하지 않음
        WolConfig(const WolConfig& other) = delete;

        /// @brief 이동 생성자 - 사용하지 않음
        WolConfig(WolConfig&& other) noexcept = delete;

        /// @brief 복사 대입 연산자 - 사용하지 않음
        WolConfig& operator=(const WolConfig& other) = delete;

        /// @brief 이동 대입 연산자 - 사용하지 않음
        WolConfig& operator=(WolConfig&& other) noexcept = delete;

        /// @brief 소멸자 - 기본 소멸자 사용
        ~WolConfig() = default;

        /// @brief 지정된 INI 파일에서 설정을 로드
        /// @return 설정 로드 및 유효성 검사 성공 시 WolErrorCode::Success, 실패 시 적절한 WolErrorCode 값
        /// @post 성공 시 mMacAddress, BroadcastIp, mPort에 로드된 값들이 저장됨
        /// @details INI 파일 구조:
        ///          [Target] 섹션 하위에 다음 키들이 존재해야 함
        ///          - MacAddress: 대상 장치의 MAC 주소 (필수)
        ///          - BroadcastIp: 브로드캐스트 IP 주소 (기본값: 255.255.255.255)
        ///          - Port: WOL 패킷 전송 포트 (기본값: 9)
        /// @note 로드된 설정값들의 유효성을 검증
        ///       - MAC 주소가 비어있지 않은지 확인, 유효하지 않음 문자가 포함되어 있지 않는지, 양식에 맞는지
        ///       - 브로드캐스트 IP가 비어있지 않은지 확인, IP 주소에 유효하지 않은 문자가 포함되어 있는지, 양식에 맞는지
        ///       - 포트 번호가 유효한지 확인 (0 < port < 65535(UINT16_MAX))
        /// @warning 모든 예외는 내부에서 처리되며 false 반환으로 오류 표시
        [[nodiscard]] WolErrorCode LoadFromIni() noexcept;

        /// @brief 설정에 저장된 MAC 주소를 반환
        /// @return 저장된 MAC 주소 문자열 (예: "00:11:22:AA:BB:CC"), 설정 파일이 유효하지 않다면 빈 문자열을 반환함
        [[nodiscard]] std::wstring GetMacAddress() const noexcept { return mMacAddress; }

        /// @brief 설정에 저장된 브로드캐스트 IP 주소를 반환
        /// @return 저장된 브로드캐스트 주소 문자열 (예: "255.255.255.255"), 설정 파일이 유효하지 않다면 빈 문자열을 반환함
        [[nodiscard]] std::wstring GetBroadcastIp() const noexcept { return mBroadcastIp; }

        /// @brief 설정에 저장된 포트 번호를 반환
        /// @return 저장된 포트 번호 (1~65535(UINT16_MAX) 범위의 16비트 정수), 설정 파일이 유효하지 않다면 유효하지 않은 포트(0)를 반환함
        [[nodiscard]] std::uint16_t GetPort() const noexcept { return mPort; }

    private:
        /// @brief 실행 파일 위치를 기반으로 설정 파일 절대 경로를 가져옴
        ///	@param configFilePath 설정 파일의 전체 경로 (예: "C:\WOL\config.ini")
        /// @return 설정 파일 절대 경로를 얻어올 수 있으면 WolErrorCode::Success, 아니라면 적절한 WolErrorCode 값
        /// @details 현재 실행중인 프로그램의 디렉토리에서 config.ini 파일 경로 생성
        ///          - GetModuleFileNameW API를 사용하여 실행 파일 경로 획득
        ///          - 디렉토리 부분만 추출하여 설정 파일명과 결합
        ///			 - 설정 파일 절대 경로를 얻는데 실패한 경우 configFilePath은 빈 문자열
        /// @warning 반환된 경로의 파일 존재 여부는 별도로 확인 필요
        [[nodiscard]] WolErrorCode GetConfigFilePath(_Out_ std::wstring& configFilePath) const noexcept;

        /// @brief 로드된 설정 매개변수들의 유효성을 검증
        /// @return 모든 설정 매개변수가 유효한 경우 true, 그렇지 않으면 false
        /// @details 포괄적인 검증 항목들:
        ///          - MAC 주소: 길이, 형식, 유효 문자 검사
        ///          - 브로드캐스트 IP: 길이, 형식, 유효 문자 검사
        ///          - 포트 번호: 유효 범위 검사 (1-65535(UINT16_MAX))
        /// @note 실제 네트워크 연결성이나 장치 존재 여부는 확인하지 않음
        ///       형식적 유효성만을 검증하여 기본적인 오류를 사전 차단
        [[nodiscard]] WolErrorCode IsConfigurationValid() const noexcept;

        /// @brief MAC 주소 형식의 유효성을 검증
        /// @param macAddress 검증할 MAC 주소 문자열
        /// @return MAC 주소가 유효한 형식인 경우 true, 그렇지 않으면 false
        /// @details 지원하는 MAC 주소 형식:
        ///          - "XX-XX-XX-XX-XX-XX" (하이픈 구분자)
        ///          - 여기서 XX는 16진수 값 (0-9, A-F, a-f)
        /// @note 대소문자를 구분하지 않으며, ':' 구분자, 혼합된 구분자는 허용하지 않음
        [[nodiscard]] WolErrorCode IsValidMacAddress(_In_ std::wstring_view macAddress) const noexcept;

        /// @brief IP 주소 형식의 유효성을 검증
        /// @param broadcastIpAddress 검증할 브로드캐스트 Ip 주소 문자열
        /// @return 브로드캐스트 Ip 주소가 유효한 IPv4 형식인 경우 WolErrorCode::Success, 그렇지 않으면 적절한 오류 코드
        /// @details IPv4 주소 형식 검증:
        ///          - "A.B.C.D" 형식 (점으로 구분된 4개의 8비트 값)
        ///          - 각 옥텟은 0-255 범위의 정수
        ///          - 선행 0은 허용하지 않음 (예: "01.02.03.04"는 무효)
        /// @note IPv6 주소는 지원하지 않음
        [[nodiscard]] WolErrorCode IsValidBroadcastIpAddress(_In_ std::wstring_view broadcastIpAddress) const noexcept;

        /// @brif 브로드캐스트 Ip 주소의 개별 옥텟을 검증
        ///	@param octet 검증할 옥텟 문자열 (예: "255")
        ///	@return 유효하다면 WolErrorCode::success, 그렇지 않다면 적합한 WolErrorCode 값
        [[nodiscard]] WolErrorCode IsValidateIpOctet(_In_ std::wstring_view octet) const noexcept;

    private:
        /// @brief INI 파일 읽기 작업을 위한 최대 버퍼 크기
        /// @details GetPrivateProfileStringW API 호출 시 사용되는 버퍼 크기 제한
        ///          일반적인 설정값 길이를 고려하여 256바이트로 설정
        ///          이 크기를 초과하는 설정값은 잘릴 수 있음
        static constexpr std::size_t MAX_BUFFER_SIZE{256U};

        /// @brief Windows 경로의 최대 길이 제한
        /// @details GetModuleFileNameW API에서 사용하는 경로 버퍼 크기
        ///          Windows MAX_PATH 상수(260자)를 사용
        /// @note 긴 경로는 지원하지 않음
        static constexpr std::size_t MAX_PATH_LENGTH{MAX_PATH};

        /// @brief MAC 주소의 최대 허용 길이
        /// @details 표준 MAC 주소 형식들의 최대 길이:
        ///          - "XX:XX:XX:XX:XX:XX" (17자)
        ///          - "XX-XX-XX-XX-XX-XX" (17자)
        ///          - 기본 17 + 1(null terminator)로 18자
        static constexpr std::size_t MAC_ADDRESS_LENGTH{18U};

        /// @brief IP 주소의 최대 허용 길이
        /// @details IPv4 주소의 최대 길이:
        ///          - "255.255.255.255" (15자)
        ///			 - 기본 15 + 1(null terminator)로 16자
        static constexpr std::size_t IP_ADDRESS_LENGTH{16U};

        /// @brief 최대 유효 포트 번호
        /// @details TCP/UDP 포트 번호의 유효 범위: 1-65535
        ///          0번 포트는 예약되어 있어 사용 불가
        static constexpr std::uint32_t MAX_VALID_PORT{UINT16_MAX};

        /// @brief 기본 WOL 포트
        ///	@details 9번 포트가 WOL 시 일반적으로 사용되는 포트
        static constexpr std::uint16_t DEFAULT_PORT{9U};

        /// @brief 대상 장치의 MAC 주소
        /// @details Wake-on-LAN 패킷을 전송할 네트워크 인터페이스의 하드웨어 주소
        ///          일반적으로 "XX:XX:XX:XX:XX:XX" 또는 "XX-XX-XX-XX-XX-XX" 형식
        ///          실제 패킷 생성 시 바이너리 형태로 변환되어 사용됨
        std::wstring mMacAddress{};

        /// @brief WOL 패킷 전송을 위한 브로드캐스트 IP 주소
        /// @details 네트워크 세그먼트 내 모든 장치에게 패킷을 전송하기 위한 주소
        ///          일반적으로 "255.255.255.255" (전역 브로드캐스트) 또는
        ///          서브넷 브로드캐스트 주소 (예: "192.168.1.255") 사용
        ///          네트워크 토폴로지에 따라 적절한 주소 선택 필요
        std::wstring mBroadcastIp{};

        /// @brief WOL 패킷 전송을 위한 대상 포트 번호 (기본값: 9)
        /// @details Wake-on-LAN 표준에서 권장하는 포트는 9번 (Discard Protocol)
        ///          일부 환경에서는 7번 (Echo) 또는 다른 포트 사용 가능
        ///          UDP 프로토콜을 사용하여 브로드캐스트 전송
        ///          유효한 포트 범위: 1-65535 (0번 포트는 예약됨)
        ///			 초기화 시에는 유효하지 않은 값으로 초기화
        std::uint16_t mPort{0};
    };

    WolErrorCode WolConfig::GetConfigFilePath(_Out_ std::wstring& configFilePath) const noexcept
    {
        configFilePath.clear();

        // 실행 파일 경로 저장을 위한 버퍼 (null 문자로 초기화)
        std::array<wchar_t, MAX_PATH_LENGTH> buffer{};

        // API 호출 실패 또는 경로가 버퍼 크기를 초과하여 잘린 경우
        if (const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            // 현재 모듈(실행 파일)의 전체 경로를 가져옴
            length == 0 || length >= MAX_PATH_LENGTH)
        {
            std::ignore = ::fwprintf(stderr, L"실행 파일 경로를 얻을 수 없습니다.\n");
            return WolErrorCode::FailedToGetExecutionPath;
        }

        try
        {
            // 실행 파일 절대 경로를 담은 buffer를 std::filesystem::path로 변환
            const std::filesystem::path exePath{buffer.data()};

            // 실행 파일이 있는 폴더 경로
            const std::filesystem::path exeDir = exePath.parent_path();

            // 디렉토리 정보가 비어있다면 파일명만 반환
            if (exeDir.empty())
            {
                // 일반적으로 발생하지 않지만, 디렉토리 경로가 비어있는 경우를 대비
                std::ignore = ::fwprintf(stderr, L"실행 파일 경로가 유효하지 않습니다: %ls\n", exeDir.c_str());
                return WolErrorCode::InvalidExecutionPath;
            }
            // 디렉토리 경로와 설정 파일명 결합
            configFilePath = (exeDir / CONFIG_FILE_NAME).wstring();
            return WolErrorCode::Success;
        }
        catch (...)
        {
            std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 설정 파일을 찾는 중 알 수 없는 오류가 발생했습니다.\n");
            return WolErrorCode::UnexpectedException;
        }
    }

    WolErrorCode WolConfig::LoadFromIni() noexcept
    {
        std::wstring configFileAbsolutePath{}; // 설정 파일 절대 경로
        const WolErrorCode errorCode = GetConfigFilePath(configFileAbsolutePath); // 설정 파일 경로를 얻어온다.
        if (errorCode != WolErrorCode::Success)
        {
            return errorCode;
        }

        // 설정파일이 존재하는지 확인
        if (std::filesystem::exists(configFileAbsolutePath) == false)
        {
            // iniPath.data()는 std::wstring_view 기반으로 null-terminated 보장이 없음 > %ls 포맷에 전달하면 미정의 동작 발생 가능
            // std::wstring_view에서 std::wstring으로 명시적 변환 후 출력
            std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 파일을 찾을 수 없습니다: %ls\n", configFileAbsolutePath.c_str());
            return WolErrorCode::ConfigFileNotFound;
        }

        try
        {
            // INI 파일 읽기를 위한 임시 버퍼 (null 문자로 초기화)
            std::array<wchar_t, MAX_BUFFER_SIZE> buffer{};

            // MAC 주소, 브로드캐스트 Ip, 포트 번호 등이 있는 섹션 명
            constexpr const wchar_t* const section = L"Target";

            // MAC 주소 로드
            // [Target] 섹션의 MacAddress 키에서 값을 읽어옴
            // 키가 없거나 값이 비어있을 경우 macResult == 0 > 유효하지 않은 맥 주소로 처리
            const DWORD macResult = ::GetPrivateProfileStringW(
                section, // 섹션명
                L"MacAddress", // 키명
                L"", // 기본값 (빈 문자열)
                buffer.data(), // 결과 저장 버퍼
                MAX_BUFFER_SIZE, // 버퍼 크기
                configFileAbsolutePath.data() // INI 파일 경로
            );

            // MAC 주소 로드 실패 시 (키가 없거나 값이 비어있음)
            if (macResult == 0U)
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 파일에서 MacAddress 키 값을 읽는데 실패했습니다.\n");
                return WolErrorCode::FailedToReadMacAddress;
            }

            // 로드된 MAC 주소를 멤버 변수에 저장
            mMacAddress.assign(buffer.data());

            // 브로드캐스트 IP 주소 로드
            // [Target] 섹션의 BroadcastIP 키에서 값을 읽어옴
            // 키가 없거나 값이 비어있는 경우 ipResult == 0 > 유효하지 않은 브로드캐스트 주소로 처리
            const DWORD ipResult = ::GetPrivateProfileStringW(
                section, // 섹션명
                L"BroadcastIp", // 키명
                L"255.255.255.255", // 기본값 (전역 브로드캐스트)
                buffer.data(), // 결과 저장 버퍼
                MAX_BUFFER_SIZE, // 버퍼 크기
                configFileAbsolutePath.data() // INI 파일 경로
            );

            // 브로드캐스트 IP 로드 실패 시
            if (ipResult == 0U)
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 파일에서 BroadcastIp 키 값을 읽는데 실패했습니다.\n");
                return WolErrorCode::FailedToReadBroadcastIp;
            }

            // 로드된 브로드캐스트 IP를 멤버 변수에 저장
            mBroadcastIp.assign(buffer.data());

            // 포트 번호 로드
            // [Target] 섹션의 Port 키에서 값을 읽어옴
            // Port 키 값이 비어있는 경우 portResult == 0 > 유효하지 않은 포트로 처리
            const DWORD portResult = ::GetPrivateProfileStringW(
                section,
                L"Port", L"",
                buffer.data(),
                MAX_BUFFER_SIZE,
                configFileAbsolutePath.data());
            if (portResult == 0U)
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 파일에서 Port 키 값을 읽는데 실패했습니다.\n");
                return WolErrorCode::FailedToReadPort;
            }

            if (std::wcslen(buffer.data()) == 0)
                return WolErrorCode::InvalidPort;

            errno = 0;
            wchar_t* endPtr = nullptr;
            const unsigned long port = wcstoul(buffer.data(), &endPtr, 10);
            if (errno == ERANGE)
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 파일에서 Port 값을 정수로 변환하는데 실패하였습니다: %ls\n",
                                         buffer.data());
                return WolErrorCode::InvalidPort;
            }

            // endPtr == buffer.data() > 변환된 숫자가 없음 (예: "abc")
            // *endPtr != L'\0' > 숫자 뒤에 쓰레기 문자 있음 (예: "255abc")
            if (endPtr == buffer.data() || *endPtr != L'\0')
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 파일의 Port 값이 유효하지 않습니다: %ls\n", buffer.data());
                return WolErrorCode::InvalidPort;
            }

            if (port == 0 || port > UINT16_MAX)
            {
                std::ignore = ::fwprintf(
                    stderr, CONFIG_FILE_NAME L" 설정 파일의 Port 키 값이 유효하지 않습니다.\n\t유효한 포트의 범위: 1 ~ 65535\n\t입력된 포트: %d\n",
                    port);
                return WolErrorCode::InvalidPort;
            }


            // 유효한 포트 값을 멤버 변수에 저장
            mPort = static_cast<std::uint16_t>(port);

            // 로드된 모든 설정값들의 최종 유효성 검사
            const WolErrorCode isValidConfiguration = IsConfigurationValid();
            if (isValidConfiguration != WolErrorCode::Success)
            {
                // Config.ini 파일이 유효하지 않으면 모든 변수 초기화
                mMacAddress = {};
                mBroadcastIp = {};
                mPort = 0;

                return isValidConfiguration;
            }
            return WolErrorCode::Success;
        }
        catch (const std::exception& e)
        {
            // 모든 예외 처리 (예: 문자열 조작 중 std::bad_alloc 등)
            std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 설정 파일 로드 중 오류가 발생했습니다: %hs\n", e.what());
            return WolErrorCode::UnexpectedException;
        }
        catch (...)
        {
            // noexcept 보장을 위해 모든 예외를 포착하고 실패로 처리
            std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 설정 파일 로드 중 알 수 없는 오류가 발생했습니다.\n");
            return WolErrorCode::UnexpectedException;
        }
    }

    WolErrorCode WolConfig::IsConfigurationValid() const noexcept
    {
        // MAC 주소 형식 유효성 검사
        WolErrorCode result = IsValidMacAddress(mMacAddress);
        if (result != WolErrorCode::Success)
        {
            return result;
        }

        // 브로드캐스트 IP 주소 형식 유효성 검사
        result = IsValidBroadcastIpAddress(mBroadcastIp);
        if (result != WolErrorCode::Success)
        {
            return result;
        }

        // 포트는 INI 파일을 읽는 위치에서 검증

        return WolErrorCode::Success;
    }

    WolErrorCode WolConfig::IsValidMacAddress(_In_ const std::wstring_view macAddress) const noexcept
    {
        const std::size_t length = macAddress.length();

        // MAC 주소 주소 문자열 길이가 MAC_ADDRESS_LENGTH(18)이 아니라면 유효하지 않은 것으로 판단
        if (macAddress.empty() || length > MAC_ADDRESS_LENGTH)
        {
            std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" 설정 파일의 MacAddress 키 값의 길이가 유효하지 않습니다.\n");
            return WolErrorCode::InvalidMacAddress;
        }

        // 각 위치별 문자 검증
        for (std::size_t i = 0U; i < length; ++i)
        {
            const wchar_t ch = macAddress.at(i);

            // 구분자
            constexpr wchar_t separator = L'-';

            // 구분자 위치
            static constexpr std::array<std::size_t, 5> MAC_ADDRESS_SEPARATOR_INDICES{2U, 5U, 8U, 11U, 14U};
            if (std::find(MAC_ADDRESS_SEPARATOR_INDICES.begin(), MAC_ADDRESS_SEPARATOR_INDICES.end(), i) !=
                MAC_ADDRESS_SEPARATOR_INDICES.end())
            {
                // 구분자 일관성 검사
                if (ch != separator)
                {
                    std::ignore = ::fwprintf(
                        stderr,
                        CONFIG_FILE_NAME L" 설정 파일의 MacAddress 키 값의 구분자가 유효하지 않습니다.\n\t유효한 구분자: '-'\n\t입력된 구분자: %c\n",
                        separator);
                    return WolErrorCode::InvalidMacAddress;
                }
            }
            else
            {
                // 16진수 문자 검사
                if (((ch >= L'0' && ch <= L'9')
                    || (ch >= L'A' && ch <= L'F')
                    || (ch >= L'a' && ch <= L'f')) == false)
                {
                    std::ignore = ::fwprintf(
                        stderr, CONFIG_FILE_NAME L" 설정 파일의 MacAddress 키 값에 유효하지 문자가 포함되어 있습니다: %c\n", ch);
                    return WolErrorCode::InvalidMacAddress;
                }
            }
        }

        return WolErrorCode::Success;
    }

    WolErrorCode WolConfig::IsValidBroadcastIpAddress(_In_ const std::wstring_view broadcastIpAddress) const noexcept
    {
        std::size_t start = 0U;
        std::size_t dotCount = 0U;

        // 점(.)으로 구분된 각 옥텟 검사
        for (std::size_t i = 0U; i <= broadcastIpAddress.length(); ++i)
        {
            // 점을 만나거나 문자열 끝에 도달한 경우
            if (i == broadcastIpAddress.length() || broadcastIpAddress[i] == L'.')
            {
                try
                {
                    const std::wstring_view octet = broadcastIpAddress.substr(start, i - start);
                    const WolErrorCode result = IsValidateIpOctet(octet);
                    if (result != WolErrorCode::Success)
                    {
                        return result;
                    }
                }
                catch (const std::exception& e)
                {
                    std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟 파싱 중 오류가 발생했습니다: %hs\n",
                                             e.what());
                    return WolErrorCode::UnexpectedException;
                }
                catch (...)
                {
                    std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟 파싱 중 알 수 없는 오류가 발생했습니다.\n");
                    return WolErrorCode::UnexpectedException;
                }

                start = i + 1U;

                // 점인 경우 카운트 증가
                if (i < broadcastIpAddress.length())
                {
                    ++dotCount;
                }

                if (dotCount > 3)
                {
                    std::ignore = ::fwprintf(
                        stderr, CONFIG_FILE_NAME L" 설정 파일의 BroadcastIp 구분자(.) 개수가 잘못되었습니다.(필요 갯수보다 많음)\n");
                    return WolErrorCode::InvalidBroadcastIp;
                }
            }
        }

        // 정확히 3개의 점이 있어야 함 (4개 옥텟)
        if (dotCount != 3U)
        {
            std::ignore = ::fwprintf(
                stderr, CONFIG_FILE_NAME L" 설정 파일의 BroadcastIp 구분자(.) 개수가 잘못되었습니다.(필요: 3, 실제: %zu)\n", dotCount);
            return WolErrorCode::InvalidBroadcastIp;
        }

        return WolErrorCode::Success;
    }

    WolErrorCode WolConfig::IsValidateIpOctet(_In_ const std::wstring_view octet) const noexcept
    {
        // 옥텟 길이 검사 (0 또는 너무 김)
        if (octet.empty() || octet.length() > 3)
        {
            std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟 길이가 유효하지 않습니다.\n");
            return WolErrorCode::InvalidBroadcastIp;
        }

        // 숫자 확인
        for (const wchar_t ch : octet)
        {
            if (iswdigit(ch) == false)
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟에 유효하지 않은 문자가 포함되어 있습니다: %c\n", ch);
                return WolErrorCode::InvalidBroadcastIp;
            }
        }

        // 선행 0 금지 (두 자리 이상인데 0으로 시작)
        if (octet.length() > 1 && octet[0] == L'0')
        {
            std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟에 선행 0이 있습니다. 지원하지 않습니다.\n");
            return WolErrorCode::InvalidBroadcastIp;
        }

        // 숫자 범위 확인
        try
        {
            errno = 0;
            wchar_t* endPtr = nullptr;
            const long value = ::wcstol(std::wstring{octet}.c_str(), &endPtr, 10);

            if (errno == EINVAL || errno == ERANGE)
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟 값을 정수로 변환하는데 실패하였습니다: %ld\n",
                                         value);
                return WolErrorCode::InvalidBroadcastIp;
            }

            // endPtr == octet.c_str() > 변환된 숫자가 없음 (예: "abc")
            // *endPtr != L'\0' > 숫자 뒤에 쓰레기 문자 있음 (예: "255abc")
            if (endPtr == octet.data() || *endPtr != L'\0')
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟 값이 유효하지 않습니다: %.*ls\n",
                                         static_cast<int>(octet.length()), octet.data());
                return WolErrorCode::InvalidBroadcastIp;
            }

            if (value < 0L || value > 255L)
            {
                std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟 값이 유효 범위를 벗어났습니다. (0~255): %ld\n",
                                         value);
                return WolErrorCode::InvalidBroadcastIp;
            }
        }
        catch (...)
        {
            std::ignore = ::fwprintf(stderr, CONFIG_FILE_NAME L" BroadcastIp 옥텟 숫자 변환 중 오류가 발생하였습니다.\n");
            return WolErrorCode::UnexpectedException;
        }

        return WolErrorCode::Success;
    }

    /// @brief WinSock SOCKET 리소스를 RAII 방식으로 관리하는 클래스
    /// @details 소멸자에서 closesocket()을 자동 호출하여 자원 누수를 방지
    class Socket final
    {
    public:
        // socket 유효한 SOCKET 핸들 또는 INVALID_SOCKET (기본값)
        /// @note 기본값은 INVALID_SOCKET이며, 이후 Set()으로 설정할 수 있음
        explicit Socket(const SOCKET& socket = INVALID_SOCKET) noexcept;

        /// @brief 소멸자
        /// @details 소켓이 유효하다면 closesocket() 호출 후 INVALID_SOCKET으로 초기화
        ~Socket() noexcept;

        /// @brief 복사 생성자 - 사용하지 않음
        Socket(const Socket& other) = delete;

        /// @brief 이동 생성자 - 사용하지 않음
        Socket(Socket&& other) noexcept = delete;

        /// @brief 복사 대입 연산자 - 사용하지 않음
        Socket& operator=(const Socket& other) = delete;

        /// @brief 이동 대입 연산자 - 사용하지 않음
        Socket& operator=(Socket&& other) noexcept = delete;

        /// @brief 새로운 SOCKET 값을 설정
        /// @param socket 새로 할당받은 유효한 소켓 핸들
        /// @note 이전 소켓이 열려 있다면 closesocket()으로 닫고 교체
        void Set(const SOCKET& socket) noexcept;

        /// @brief 내부의 raw SOCKET 핸들을 반환
        [[nodiscard]] SOCKET Get() const noexcept { return mSocket; }

    private:
        /// @brief 내부 소켓 핸들을 닫고 INVALID_SOCKET으로 초기화
        /// @details 유효한 경우에만 closesocket()을 호출
        void Close() noexcept;

    private:
        /// @brief WinSock에서 사용되는 SOCKET 핸들
        SOCKET mSocket;
    };

    Socket::Socket(const SOCKET& socket) noexcept
        : mSocket(socket)
    {
    }

    Socket::~Socket() noexcept
    {
        // WinSock SOCKET은 자동 관리되지 않는 리소스이므로 명시적으로 closesocket() 필요
        // Rule of Zero는 이 경우 적용되지 않음
        Close();
    }


    void Socket::Set(const SOCKET& socket) noexcept
    {
        assert(socket != INVALID_SOCKET); // 유효하지 않은 핸들은 허용하지 않음

        Close(); // 이전 핸들이 열려 있다면 닫음
        mSocket = socket; // 새 핸들로 교체
    }

    void Socket::Close() noexcept
    {
        if (mSocket == INVALID_SOCKET)
            return;

        closesocket(mSocket); // 소켓 닫기
        mSocket = INVALID_SOCKET; // 상태 초기화
    }

    /// @brief WinSock 초기화/해제를 보장하는 RAII 유틸리티 클래스
    /// @details WSAStartup()을 호출해 WinSock 환경을 초기화하고
    ///			소멸자에서 WSACleanup()을 자동 호출
    ///			단일 스레드 또는 모듈 내에서 중복 초기화를 방지하기 위해 참조 카운트를 사용
    class WsaGuard final
    {
    public:
        /// @brief 기본 생성자
        WsaGuard() noexcept = default;

        /// @brief 복사 생성자 - 사용하지 않음
        WsaGuard(const WsaGuard& other) = delete;

        /// @brief 이동 생성자 - 사용하지 않음
        WsaGuard(WsaGuard&& other) noexcept = delete;

        /// @brief 복사 대입 연산자 - 사용하지 않음
        WsaGuard& operator=(const WsaGuard& other) = delete;

        /// @brief 이동 대입 연산자 - 사용하지 않음
        WsaGuard& operator=(WsaGuard&& other) noexcept = delete;

        /// @brief 소멸자
        /// @details 참조 카운트를 감소시키고, 0이 되면 WSACleanup() 호출
        ~WsaGuard() noexcept;

        /// @brief WinSock 환경을 초기화합니다.
        /// @return 성공 시 WolErrorCode::Success, 실패 시 적절한 에러 코드
        /// @note 참조 카운트를 증가시켜 중복 초기화를 안전하게 처리
        WolErrorCode Initialize();

    private:
        /// @brief WSAStartup()에서 채워지는 구조체
        WSADATA mWsaData{};

        /// @brief 현재 활성화된 초기화 인스턴스의 수
        /// @note 전역 정적 변수이며, Initialize() 성공 시 증가, 소멸자에서 감소
        static unsigned long long mRefCount;
    };

    // 정적 참조 카운트 초기값
    unsigned long long WsaGuard::mRefCount = 0;

    WsaGuard::~WsaGuard() noexcept
    {
        --mRefCount;
        if (mRefCount == 0)
        {
            // 모든 인스턴스가 종료되었으므로 WinSock 환경 종료
            std::ignore = WSACleanup(); // WSACleanup() 실패 시 복구 수단이 없으므로 무시
        }
    }

    WolErrorCode WsaGuard::Initialize()
    {
        // WinSock 초기화
        if (WSAStartup(MAKEWORD(2, 2), &mWsaData) != 0)
        {
            std::ignore = ::fwprintf(stderr, L"WinSock 초기화에 실패했습니다.\n");
            return WolErrorCode::WinsockInitializationFailed;
        }

        // 성공 시 참조 카운트 증가
        mRefCount++;

        return WolErrorCode::Success;
    }

    ///	@brief WOL 패킷 전송 클래스
    class WakeOnLanSender final
    {
    public:
        /// @brief 기본 생성자
        WakeOnLanSender() noexcept = default;

        /// @brief 복사 생성자 - 사용하지 않음
        WakeOnLanSender(const WakeOnLanSender& other) = delete;

        /// @brief 이동 생성자 - 사용하지 않음
        WakeOnLanSender(WakeOnLanSender&& other) noexcept = delete;

        /// @brief 복사 대입 연산자 - 사용하지 않음
        WakeOnLanSender& operator=(const WakeOnLanSender& other) = delete;

        /// @brief 이동 대입 연산자 - 사용하지 않음
        WakeOnLanSender& operator=(WakeOnLanSender&& other) noexcept = delete;

        /// @brief 소멸자 - 기본 소멸자 사용
        ~WakeOnLanSender() noexcept = default;

        ///	@brief WOL 매직 패킷을 전송합니다.
        ///	@param macAddress 대상 장치의 MAC 주소 (예: "00:11:22:AA:BB:CC")
        ///	@param broadcastAddress 브로드캐스트 주소
        ///	@param port 포트 번호
        ///	@return 전송에 성공한 경우 WolErrorCode::Success, 실패한 경우 적절한 WolErrorCode 값
        [[nodiscard]] WolErrorCode SendMagicPacket(_In_ std::wstring_view macAddress,
                                                   _In_ std::wstring_view broadcastAddress,
                                                   _In_range_(1, 65535) std::uint16_t port) const noexcept;

    private:
        /// @brief MAC 주소 문자열을 바이트 배열로 변환
        /// @param macAddressString 변환할 MAC 주소 문자열 (예: "00:11:22:AA:BB:CC")
        /// @param macBytes 변환된 MAC 주소 바이트 배열 출력
        /// @note 입력 문자열은 "XX-XX-XX-XX-XX-XX" 형식을 따라야 하며, 각 XX는 00~FF 범위의 16진수여야 함
        void ParseMacAddress(_In_ std::wstring_view macAddressString, _Out_ MacAddress& macBytes) const noexcept;

        /// @brief 매직 패킷을 생성
        /// @param macBytes 대상 장치의 MAC 주소 바이트 배열
        /// @param packet 생성된 매직 패킷(102바이트) 출력
        /// @details 첫 6바이트는 0xFF 동기화 헤더로, 이후 MAC 주소를 16회 반복하여 패킷을 구성
        void CreateMagicPacket(_In_ const MacAddress& macBytes, _Out_ MagicPacket& packet) const noexcept;

        /// @brief UDP 소켓을 초기화
        /// @param socket 생성된 소켓 핸들이 저장될 변수
        /// @return 초기화 성공 시 WolErrorCode::Success, 실패 시 적절한 WolErrorCode 값
        /// @note AF_INET, SOCK_DGRAM, IPPROTO_UDP 옵션으로 소켓을 생성합니다.
        [[nodiscard]] WolErrorCode InitializeSocket(_Inout_ Socket& socket) const noexcept;

        /// @brief 브로드캐스트 대상 주소를 설정
        /// @param broadcastAddress 브로드캐스트 IP 문자열 (예: "255.255.255.255")
        /// @param port 대상 포트 번호 (1~65535)
        /// @param destAddr 설정된 sockaddr_in 구조체 출력
        /// @return 변환 성공 시 WolErrorCode::Success, 실패 시 적절한 WolErrorCode 값
        /// @details WideCharToMultiByte로 문자열을 ANSI로 변환한 후 inet_pton으로 sin_addr를 설정
        [[nodiscard]] WolErrorCode SetupBroadcastAddress(_In_ std::wstring_view broadcastAddress,
                                                         _In_range_(1, 65535) std::uint16_t port,
                                                         _Out_ sockaddr_in& destAddr) const noexcept;
    };

    inline WolErrorCode WakeOnLanSender::SendMagicPacket(_In_ const std::wstring_view macAddress,
                                                         _In_ const std::wstring_view broadcastAddress,
                                                         _In_range_(1, 65535) const std::uint16_t port) const noexcept
    {
        // 설정 파일을 읽는 과정에서 설정 값(Mac Address, Broadcast Address, port)의 값이 유효한지
        // 검증 했기 떄문에 여기서 또 검증하지 않는다. 간단히 assert로만 체크
        assert(macAddress.empty() == false);
        assert(broadcastAddress.empty() == false);
        assert(port != 0);

        // MAC 주소 파싱
        MacAddress macBytes{};
        ParseMacAddress(macAddress, macBytes);

        // 매직 패킷 생성
        MagicPacket packet{};
        CreateMagicPacket(macBytes, packet);

        WsaGuard wsaGuard;
        WolErrorCode wolErrorCode = wsaGuard.Initialize();
        if (wolErrorCode != WolErrorCode::Success)
        {
            return wolErrorCode;
        }

        // 소켓 초기화
        Socket socket;
        wolErrorCode = InitializeSocket(socket);
        if (wolErrorCode != WolErrorCode::Success)
        {
            return wolErrorCode;
        }

        // 브로드캐스트 설정
        constexpr bool broadcastOpt = true;
        if (setsockopt(socket.Get(), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcastOpt),
                       sizeof(broadcastOpt)) == SOCKET_ERROR)
        {
            return WolErrorCode::BroadcastSetupFailed;
        }

        // 대상 주소 설정
        sockaddr_in destAddr{};
        wolErrorCode = SetupBroadcastAddress(broadcastAddress, port, destAddr);
        if (wolErrorCode != WolErrorCode::Success)
        {
            return wolErrorCode;
        }

        // 매직 패킷 전송
        const int sendResult = sendto(socket.Get(), reinterpret_cast<const char*>(packet.data()),
                                      static_cast<int>(packet.size()), 0, reinterpret_cast<const sockaddr*>(&destAddr),
                                      sizeof(destAddr));
        if (sendResult == SOCKET_ERROR)
        {
            std::ignore = ::fwprintf(stderr, L"패킷 전송 실패: %d (WSALastError)\n", WSAGetLastError());
            return WolErrorCode::PacketSendFailed;
        }

        return WolErrorCode::Success;
    }

    inline void WakeOnLanSender::ParseMacAddress(_In_ const std::wstring_view macAddressString,
                                                 _Out_ MacAddress& macBytes) const noexcept
    {
        // 설정 파일을 읽는 과정에서 설정 값(Mac Address, Broadcast Address, port)의 값이 유효한지
        // 검증 했기 떄문에 여기서 또 검증하지 않는다. 간단히 assert로만 체크
        assert(macAddressString.empty() == false);

        std::array<unsigned int, 6> tempValues{};

        // MAC 주소 파싱 (예: "A0-36-BC-BB-EB-CC")
        const std::wstring tempMacString{macAddressString}; // null 종료 보장
        [[maybe_unused]] const int scanResult = swscanf_s(tempMacString.c_str(), L"%x-%x-%x-%x-%x-%x",
                                                          &tempValues[0], &tempValues[1], &tempValues[2],
                                                          &tempValues[3], &tempValues[4], &tempValues[5]);

        // 마찬가지로 설정 파일을 읽는 과정에서 검증했기 때문에
        // 또 검증하지는 않는다.
        // 만일 파싱 과정에서 오류가 발생한다면 WolConfig::IsValidMacAddress() 함수를 보완해야 함
        assert(scanResult == 6);

        // 변환
        for (std::size_t i = 0U; i < macBytes.size(); ++i)
        {
            macBytes[i] = static_cast<std::byte>(tempValues[i]);
        }
    }

    inline void WakeOnLanSender::CreateMagicPacket(_In_ const MacAddress& macBytes,
                                                   _Out_ MagicPacket& packet) const noexcept
    {
        // 첫 6바이트를 0xFF로 설정
        constexpr std::byte magicHeader = static_cast<std::byte>(0xFF);
        constexpr std::size_t headerSize = 6;
        constexpr std::size_t macRepeatCount = 16;

        for (std::size_t i = 0U; i < headerSize; ++i)
        {
            packet[i] = magicHeader;
        }

        // MAC 주소를 16번 반복
        for (std::size_t repeat = 0U; repeat < macRepeatCount; ++repeat)
        {
            const std::size_t offset = headerSize + (repeat * macBytes.size());
            for (std::size_t i = 0U; i < macBytes.size(); ++i)
            {
                packet[offset + i] = macBytes[i];
            }
        }
    }

    inline WolErrorCode WakeOnLanSender::InitializeSocket(_Inout_ Socket& socket) const noexcept
    {
        // 소켓 생성
        const SOCKET rawSocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (rawSocket == INVALID_SOCKET)
        {
            std::ignore = ::fwprintf(stderr, L"소켓 생성에 실패 했습니다.\n");
            return WolErrorCode::SocketCreationFailed;
        }

        // ICMP "port unreachable" 리셋 방지
        bool bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        WSAIoctl(rawSocket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
                 nullptr, 0, &dwBytesReturned, nullptr, nullptr);

        socket.Set(rawSocket);
        return WolErrorCode::Success;
    }

    inline WolErrorCode WakeOnLanSender::SetupBroadcastAddress(_In_ const std::wstring_view broadcastAddress,
                                                               _In_range_(1, 65535) const std::uint16_t port,
                                                               _Out_ sockaddr_in& destAddr) const noexcept
    {
        // 설정 파일을 읽는 과정에서 설정 값(Mac Address, Broadcast Address, port)의 값이 유효한지
        // 검증 했기 떄문에 여기서 또 검증하지 않는다. 간단히 assert로만 체크
        assert(broadcastAddress.empty() == false);
        assert(port != 0 && port < UINT16_MAX);

        // 구조체 초기화
        destAddr = {};
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(port);

        // Wide char를 Multi-byte로 변환
        const int sizeNeeded = WideCharToMultiByte(CP_ACP, 0, broadcastAddress.data(),
                                                   static_cast<int>(broadcastAddress.size()), nullptr, 0, nullptr,
                                                   nullptr);
        std::string ipStr(sizeNeeded, 0);
        const int conversionResult = WideCharToMultiByte(CP_ACP, 0, broadcastAddress.data(),
                                                         static_cast<int>(broadcastAddress.size()), ipStr.data(),
                                                         sizeNeeded, nullptr, nullptr);

        if (conversionResult == 0 || conversionResult != sizeNeeded)
        {
            std::ignore = ::fwprintf(stderr, L"broadcastAddress를 Multi-byte 문자열로 변환하는데 실패하였습니다: %.*ls\n",
                                     static_cast<int>(broadcastAddress.size()), broadcastAddress.data());
            return WolErrorCode::BroadcastSetupFailed;
        }

        // IP 주소 변환
        const int ptonResult = inet_pton(AF_INET, ipStr.c_str(), &destAddr.sin_addr);
        if (ptonResult != 1)
        {
            std::ignore = ::fwprintf(stderr, L"broadcastAddress를 IP로 변환하는데 실패하였습니다: %.*ls\n",
                                     static_cast<int>(broadcastAddress.size()), broadcastAddress.data());
            return WolErrorCode::BroadcastSetupFailed;
        }

        return WolErrorCode::Success;
    }
}

int main()
{
    std::ignore = _setmode(_fileno(stdout), _O_U16TEXT);
    std::ignore = _setmode(_fileno(stderr), _O_U16TEXT);

    WakeOnLan::WolConfig config;
    WakeOnLan::WolErrorCode errorCode = config.LoadFromIni();
    if (errorCode != WakeOnLan::WolErrorCode::Success)
    {
        std::ignore = ::fwprintf(stderr, L"설정 파일을 읽는데 실패했습니다.\n\t%ls",
                                 WakeOnLan::WolErrorCodeToString(errorCode).c_str());

        if (errorCode == WakeOnLan::WolErrorCode::FailedToReadMacAddress
            || errorCode == WakeOnLan::WolErrorCode::FailedToReadBroadcastIp
            || errorCode == WakeOnLan::WolErrorCode::FailedToReadPort)
        {
            std::ignore = ::fwprintf(stderr, L"\t설정 파일이 UTF-8 인코딩이 아닌 경우 발생할 수 있습니다. UTF-8 파일만 지원합니다.");
        }

        return static_cast<int>(errorCode);
    }

    std::ignore = ::fwprintf(stdout, L"=== Wake-on-LAN ===\n");
    std::ignore = ::fwprintf(stdout, L"대상 MAC: %ls\n", config.GetMacAddress().c_str());
    std::ignore = ::fwprintf(stdout, L"브로드캐스트 IP: %ls\n", config.GetBroadcastIp().c_str());
    std::ignore = ::fwprintf(stdout, L"포트: %d\n", config.GetPort());
    std::ignore = ::fwprintf(stdout, L"================================\n\n");

    const WakeOnLan::WakeOnLanSender wolSender{};
    errorCode = wolSender.SendMagicPacket(config.GetMacAddress(), config.GetBroadcastIp(), config.GetPort());

    std::ignore = ::fwprintf(stdout, L"WOL 패킷 전송 결과: %ls\n", WakeOnLan::WolErrorCodeToString(errorCode).c_str());

    if (errorCode == WakeOnLan::WolErrorCode::Success)
    {
        std::ignore = ::fwprintf(stdout, L"매직 패킷이 성공적으로 전송되었습니다!\n");
        std::ignore = ::fwprintf(stdout, L"대상 PC가 켜지지 않는다면 다음을 확인하세요:\n");
        std::ignore = ::fwprintf(stdout, L"  1. 대상 PC의 BIOS에서 Wake-on-LAN 활성화\n");
        std::ignore = ::fwprintf(stdout, L"  2. 네트워크 어댑터의 전원 관리 설정\n");
        std::ignore = ::fwprintf(stdout, L"  3. 올바른 MAC 주소 및 브로드캐스트 IP\n");
        std::ignore = ::fwprintf(stdout, L"  4. 방화벽/라우터 설정\n");
    }
    else
    {
        std::ignore = ::fwprintf(stdout, L"패킷 전송에 실패했습니다.\n");
    }

    std::ignore = ::fwprintf(stdout, L"프로그램을 종료하려면 Enter를 누르세요...");

    // Enter 키 대기
    std::wstring buffer(10, L'\0');
    _getws_s(buffer.data(), std::size(buffer));

    return static_cast<int>(errorCode);
}
