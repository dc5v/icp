# OPC DA 클라이언트 명령행 도구 사용법

## 기본 연결 정보 (CONNECTION_INFO)

| 연결 형식               | 설명               | 예시                                                     |
|---------------------|------------------|--------------------------------------------------------|
| progid              | 프로그램 ID          | Matrikon.OPC.Simulation.1                              |
| [host]/progid       | 원격 호스트 + 프로그램 ID | [192.168.1.100]/Matrikon.OPC.Simulation.1              |
| opcda://host/progid | URL 형식           | opcda://192.168.1.100/Matrikon.OPC.Simulation.1        |
| {clsid}             | CLSID            | {A879768D-6F4F-11D2-8FAC-00C04F8918AE}                 |
| [host]/{clsid}      | 원격 호스트 + CLSID   | [192.168.1.100]/{A879768D-6F4F-11D2-8FAC-00C04F8918AE} |

> **참고**: CONNECTION_INFO를 직접 사용하면 --progid, --clsid, --host 옵션이 필수가 아니게 됩니다.

## 주요 명령어

| 명령어                    | 기능           | 필수 옵션     | 예시                                                                   |
|------------------------|--------------|-----------|----------------------------------------------------------------------|
| --discovery            | 서버 검색        | -         | opcda86_cli.exe --discovery localhost                                |
| --browse-tags          | 모든 태그 조회     | 서버 ID     | opcda86_cli.exe --browse-tags Matrikon.OPC.Simulation.1              |
| --browse-tags-readable | 읽기 가능 태그 조회  | 서버 ID     | opcda86_cli.exe --browse-tags-readable Matrikon.OPC.Simulation.1     |
| --tag-values           | 태그 값 읽기      | 서버 ID, 태그 | opcda86_cli.exe --tag-values Matrikon.OPC.Simulation.1 --tag "TAG01" |
| --subscribe            | 태그 값 구독(변경시) | 서버 ID     | opcda86_cli.exe --subscribe Matrikon.OPC.Simulation.1                |
| --dialog               | 대화형 태그 검색    | 서버 ID     | opcda86_cli.exe --dialog Matrikon.OPC.Simulation.1                   |

## 데이터 열 옵션 (--data 옵션)

| 열 이름      | 약어    | 출력 결과 예시                 | 설명                         |
|-----------|-------|--------------------------|----------------------------|
| value     | v     | 88595                    | 태그 값                       |
| rvalue    | rv    | I4                       | 원시 값/타입                    |
| quality   | q     | GOOD                     | 품질 상태                      |
| rquality  | rq    | 0xc0                     | 품질 원시값                     |
| timestamp | tm    | 2025-04-28T05:21:39.006Z | 시간 ISO 8601 표현식 (UTC)      |
| epochtime | ep    | 1745816939588            | 시간 Epochtime/Unixtime (ms) |
| access    | a     | R 또는 RW                  | 접근 권한                      |
| raccess   | ra    | 0x1                      | 접근 원시값                     |
| readable  | read  | true                     | 읽기 가능 여부                   |
| writeable | write | false                    | 쓰기 가능 여부                   |

## 기능목록

### 서버 검색 (--discovery)

opcda86_cli.exe --discovery [--host <hostname>]

### 모든태그 브라우징 (--browse-tags)

opcda86_cli.exe --browse-tags [<CONNECTION_INFO>] --progid <progid> [--host <hostname>] [--status]

### (읽기권한/읽을수있는) 태그 브라우징 (--browse-tags-readable)

### 태그값 읽기 (--tag-values)

opcda86_cli.exe  <서버ID> --tag <태그1> [--tag <태그2>...]
opcda86_cli.exe --tag-values <서버ID> --tags <태그1> <태그2>...

### 실시간 태그값 모니터링 / 태그 구독모드 (--subscribe)

opcda86_cli.exe --subscribe <서버ID> [--interval <ms>] [--excludes <제외태그>...]

- --interval: 업데이트 주기(ms), 기본값: 1000
- --excludes: 모니터링에서 제외할 태그 목록

### 대화형 태그값 모니터링 (--dialog)

opcda86_cli.exe --dialog Matrikon.OPC.Simulation.1
tag: group.*   (group으로 시작하는 모든 태그 검색)
tag: OPC       (OPC가 포함된 모든 태그 검색)
tag: exit      (종료)


## 자주 사용하는 명령어 예시

bash
# 1. 서버 검색
opcda86_cli.exe --discovery

# 2. 특정 태그 값 읽기
opcda86_cli.exe --tag-values Matrikon.OPC.Simulation.1 --tag "Configured Aliases.TAG_GROUP.TAG01"

# 3. 여러 태그 동시에 읽기
opcda86_cli.exe --tag-values Matrikon.OPC.Simulation.1 --tags "TAG01" "TAG02" "TAG03"

# 4. 특정 열만 표시
opcda86_cli.exe --tag-values Matrikon.OPC.Simulation.1 --tag "TAG01" --data "value quality timestamp"

# 5. 실시간 모니터링 (500ms 주기)
opcda86_cli.exe --subscribe Matrikon.OPC.Simulation.1 --interval 500
