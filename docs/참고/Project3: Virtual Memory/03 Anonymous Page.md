# 익명 페이지

이 장에서는 디스크 기반 이미지가 아닌 익명 페이지(anonymous page)를 구현합니다。

익명 매핑(anonymous mapping)은 백업 파일이나 장치를 가지지 않습니다。 파일 기반 페이지와 달리 이름이 정해진 파일 소스가 없기 때문에 "익명"이라 부릅니다。 익명 페이지는 스택과 힙과 같이 실행 중인 프로그램의 메모리에서 사용됩니다。

익명 페이지를 설명하는 구조체는 `include/vm/anon.h`의 `anon_page`입니다。 현재는 비어 있지만、 구현을 진행하면서 익명 페이지의 상태や 필요한 정보를 저장하기 위해 멤버를 추가할 수 있습니다。 또한 페이지의 일반 정보를 담고 있는 `include/vm/page.h`의 `struct page`를 확인하세요。 익명 페이지의 경우 `struct page` 안에 `struct anon_page anon` 멤버가 포함됩니다。

## 지연 로딩(Lazy Loading)과 페이지 초기화

지연 로딩(lazy loading)은 메모리 로딩을 실제로 필요할 때까지 미루는 설계입니다。 페이지 구조체(page struct)는 할당되어 있지만 전용 물리 프레임은 아직 할당되지 않았고, 실제 페이지 내용은 로드되지 않은 상태입니다. 내용은 실제로 필요할 때(즉 페이지 폴트가 발생할 때) 로드됩니다。

페이지 타입が 세 가지이므로 초기화 루틴은 각 타입마다 다릅니다。 높은 수준에서의 페이지 초기화 흐름은 다음과 같습니다。 커널이 새로운 페이지 요청을 받을 때 `vm_alloc_page_with_initializer`가 호출됩니다。 초기화자는 페이지 구조체를 할당하고 페이지 타입에 따라 적절한 초기화자를 설정한 뒤 사용자 프로그램으로 제어를 반환합니다。 사용자 프로그램이 실행되는 동안 프로그램이 소유하고 있다고 믿는 페이지에 접근하면(그러나 내용이 없는 경우) 페이지 폴트가 발생합니다。 폴트 처리 중 `uninit_initialize`가 호출되고, 이는 이전에 설정한 초기화자를 호출합니다。 익명 페이지의 초기화자는 `anon_initializer`이고, 파일 기반 페이지의 초기화자는 `file_backed_initializer`입니다。

페이지의 생명주기는 대략 initialize -> (page_fault -> lazy-load -> swap-in -> swap-out -> ...) -> destroy로 진행됩니다。 생명주기의 각 전이에 대해 필요한 절차는 페이지 타입에 따라 다릅니다。 위의 단락은 초기화의 예시입니다。 이 프로젝트에서는 각 페이지 타입에 대해 이러한 전이 과정을 구현해야 합니다。

### 실행파일의 지연 로딩(Lazy Loading for Executable)

지연 로딩에서는 프로세스 실행 시작 시 즉시 필요한 메모리 부분만 주기억장치에 로드합니다。 이는 이ager한 로딩(eager loading)과 비교해 오버헤드를 줄여줄 수 있습니다。

지연 로딩을 지원하기 위해 `include/vm/vm.h`에 `VM_UNINIT`이라는 페이지 타입을 도입합니다。 모든 페이지는 처음에 `VM_UNINIT`으로 생성됩니다。 또한 초기화되지 않은 페이지를 위한 구조체 `struct uninit_page`가 `include/vm/uninit.h`에 제공됩니다。 초기화되지 않은 페이지의 생성、 초기화、 소멸을 위한 함수는 `include/vm/uninit.c`에서 찾을 수 있으며、 이후 이 함수들을 완성해야 합니다。

페이지 폴트 발생 시(함수 `userprog/exception.c`의 `page_fault`) 제어는 `vm/vm.c`의 `vm_try_handle_fault`로 넘어갑니다。 이 함수는 유효한 페이지 폴트인지 먼저 확인합니다。 유효하지 않은(잘못된) 폴트라면 프로세스를 종료해야 합니다。 유효한 폴트라면 로드가 필요한 경우 페이지 내용을 로드하고 사용자 프로그램으로 제어를 반환합니다。

잘못된 페이지 폴트에는 지연 로드 대상、 스왑아웃된 페이지、 쓰기 금지 페이지(Copy-on-Write 관련) 등 세 가지 케이스가 있습니다。 우선은 지연 로드(lazy-loaded) 케이스만 고려하세요。 지연 로드용 페이지 폴트인 경우、 커널은 `vm_alloc_page_with_initializer`에서 이전에 설정한 초기화자 중 하나를 호출하여 세그먼트를 지연 로드합니다。 `userprog/process.c`의 `lazy_load_segment`를 구현해야 합니다。

다음 함수를 구현하세요：

```c
bool vm_alloc_page_with_initializer (enum vm_type type, void *va,
	bool writable, vm_initializer *init, void *aux);
```

주어진 타입으로 초기화된 초기화자를 사용하여 초기화되지 않은 페이지를 생성합니다。 uninit 페이지의 `swap_in` 핸들러는 타입에 따라 페이지를 자동으로 초기화하고 주어진 `aux`를 사용해 `INIT`을 호출합니다。 페이지 구조체를 얻으면 해당 페이지를 프로세스의 보조 페이지 테이블에 삽입하세요。 `vm.h`에 정의된 `VM_TYPE` 매크로를 이용하면 편리합니다。

페이지 폴트 핸들러는 호출 체인을 따라가 결국 `swap_in`을 호출할 때 `uninit_initialize`에 도달합니다。 템플릿은 `uninit_initialize`의 완전한 구현을 제공합니다。 다만 설계에 따라 `uninit_initialize`를 수정해야 할 수도 있습니다。

`vm/anon.c`의 `vm_anon_init`과 `anon_initializer`를 필요에 따라 수정할 수 있습니다。

```c
void vm_anon_init (void);
```

익명 페이지 서브시스템을 위한 초기화 함수입니다。 이 함수에서 익명 페이지와 관련된 초기화 작업을 수행하세요。

```c
bool anon_initializer (struct page *page,enum vm_type type, void *kva);
```

이 함수는 먼저 익명 페이지의 핸들러들을 `page->operations`에 설정합니다。 현재 비어 있는 `anon_page` 구조체에 필요한 정보를 업데이트해야 할 수도 있습니다。 이 함수는 익명 페이지(`VM_ANON`)의 초기화자로 사용됩니다。

`userprog/process.c`에서 `load_segment`와 `lazy_load_segment`를 구현하세요。 실행 파일에서 세그먼트를 로드하는 작업을 구현합니다。 모든 페이지는 지연 로드되어야 하며、 이는 커널이 페이지 폴트를 가로챌 때 실제로 파일에서 내용을 읽어오는 시점입니다。

```c
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
	uint32_t read_bytes, uint32_t zero_bytes, bool writable);
```

현재 코드는 루프 내에서 파일에서 읽을 바이트 수와 0으로 채울 바이트 수를 계산한 뒤 `vm_alloc_page_with_initializer`를 호출하여 보류 중인 페이지 객체를 생성합니다。 `vm_alloc_page_with_initializer`의 `aux` 인자로 전달할 보조 정보(auxiliary values)를 설정해야 합니다。 바이너리 로딩에 필요한 정보를 담는 구조체를 만들어 사용하는 것이 편리할 수 있습니다。

```c
static bool lazy_load_segment (struct page *page, void *aux);
```

`load_segment`에서 `vm_alloc_page_with_initializer`의 네 번째 인자로 `lazy_load_segment`를 전달했을 것입니다。 이 함수는 실행파일 페이지의 초기화자로서 페이지 폴트 시 호출됩니다。 이 함수는 `page`와 `aux`를 인자로 받으며、 `aux`는 `load_segment`에서 설정한 정보를 포함합니다。 이 정보를 사용하여 파일에서 세그먼트를 읽어와 메모리에 채워야 합니다。

`userprog/process.c`의 `setup_stack`을 새로운 메모리 관리 시스템에 맞게 조정해야 합니다。 첫 번째 스택 페이지는 지연 할당할 필요가 없습니다。 명령행 인자를 포함한 첫 스택 페이지는 로드 시점에 할당하고 초기화할 수 있습니다。 스택을 식별하는 방법을 제공해야 할 수도 있습니다。 `vm/vm.h`의 `vm_type`에 있는 보조 마커들(예: `VM_MARKER_0`)을 이용해 페이지를 표시할 수 있습니다。

마지막으로 `vm_try_handle_fault`를 수정하여 보조 페이지 테이블을 `spt_find_page`로 조회함으로써 폴트가 발생한 주소에 해당하는 페이지 구조체를 해결하도록 하세요。

위의 요구사항을 모두 구현하면 프로젝트 2의 모든 테스트가 통과해야 합니다。
