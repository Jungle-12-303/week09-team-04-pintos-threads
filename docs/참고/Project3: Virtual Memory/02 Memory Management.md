# 메모리 관리

가상 메모리 시스템을 지원하려면 가상 페이지와 물리 프레임을 효과적으로 관리해야 합니다. 즉, 어느(가상 또는 물리) 메모리 영역이 누가 어떤 용도로 사용하는지 등을 추적해야 합니다. 먼저 보조 페이지 테이블(supplemental page table)을 다루고, 그 다음 물리 프레임을 다룹니다. 이해를 돕기 위해 본문에서는 "페이지(page)"는 가상 페이지를, "프레임(frame)"은 물리 페이지를 의미한다고 가정합니다.

## 페이지 구조와 연산

### struct page
`include/vm/vm.h`에 정의된 `struct page`는 가상 메모리의 페이지를 나타내는 구조체로, 해당 페이지에 대해 알아야 할 모든 데이터를 저장합니다. 템플릿에서는 현재 다음과 같이 구성되어 있습니다:

```c
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};
```

이 구조체는 페이지 연산(아래 설명), 가상 주소, 물리 프레임에 대한 포인터를 가집니다. 또한 `union` 필드를 포함합니다. `union`은 같은 메모리 영역에서 여러 타입의 데이터를 저장할 수 있게 하는 특수 자료형으로, 여러 멤버를 가질 수 있지만 동시에 값이 들어있는 멤버는 하나뿐입니다. 즉, 우리 시스템의 페이지는 `uninit_page`, `anon_page`, `file_page`, 또는 `page_cache` 중 하나의 정보를 가집니다. 예를 들어 페이지가 익명 페이지(`Anonymous Page`)라면, `struct anon_page anon` 멤버가 그 페이지에 필요한 모든 정보를 담고 있게 됩니다.

### 페이지 연산(Page Operations)
앞서 보았듯이 페이지는 `VM_UNINIT`, `VM_ANON`, `VM_FILE` 중 하나일 수 있습니다. 페이지에 대해 수행해야 할 작업에는 스왑인(swap in), 스왑아웃(swap out), 소멸(destroy) 등이 있으며, 페이지 타입에 따라 필요한 처리 단계가 다릅니다. 즉, `VM_ANON` 페이지와 `VM_FILE` 페이지에 대해 서로 다른 destroy 함수가 호출되어야 합니다。

이를 위해 각 페이지 타입별로 적절한 루틴을 런타임에 호출할 수 있도록 함수 포인터를 이용한 일종의 "클래스 상속" 패턴을 사용합니다. C 언어에 클래스나 상속은 없으므로, 함수 포인터를 이용해 동일한 개념을 구현합니다(실제 운영체제 코드(예: Linux)에서도 이와 유사한 기법을 사용합니다)。

함수 포인터는 메모리 내의 실행 가능한 코드(함수)를 가리키는 포인터입니다. 함수 포인터를 사용하면 런타임 값에 따라 별도의 검증 없이 적절한 함수를 호출할 수 있습니다. 본 과제에서는 코드 수준에서 단순히 `destroy(page)`를 호출하면, 페이지 타입에 맞는 destroy 루틴이 함수 포인터를 통해 선택되어 실행됩니다。

`struct page_operations`는 `include/vm/vm.h`에 정의되어 있으며, 3개의 함수 포인터를 포함하는 함수 테이블로 생각하면 됩니다:

```c
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};
```

예를 들어 `vm/file.c`를 보면 파일 기반 페이지(file-backed pages)를 위한 `page_operations` 테이블 `file_ops`가 함수 선언 앞에 정의되어 있습니다. 이 테이블의 `.destroy` 필드는 `file_backed_destroy`로 설정되어 있으며, 해당 함수는 같은 파일 안에서 정의되어 있습니다。

함수 포인터 인터페이스로 `file_backed_destroy`가 어떻게 호출되는지 이해해봅시다. `vm_dealloc_page(page)`(vm/vm.c)가 호출되고 해당 페이지가 파일-기반 페이지(`VM_FILE`)인 경우, 함수 내부에서 `destroy(page)`를 호출합니다. `destroy(page)`는 `include/vm/vm.h`에 매크로로 정의되어 있습니다:

```c
#define destroy(page) if ((page)->operations->destroy) (page)->operations->destroy (page)
```

이 매크로는 `destroy(page)`가 내부적으로 `(page)->operations->destroy(page)`를 호출함을 의미합니다. 페이지가 `VM_FILE` 타입이면 `.destroy`는 `file_backed_destroy`를 가리키므로 해당 페이지 전용 소멸 루틴이 실행됩니다。

## 보조 페이지 테이블 구현

현재 Pintos는 가상-물리 매핑을 관리하는 페이지 테이블(`pml4`)을 가지고 있습니다. 그러나 페이지 폴트 처리와 자원 관리를 위해서는 추가 정보가 필요하므로 보조 페이지 테이블을 구현해야 합니다. 프로젝트 3의 첫 번째 과제로 보조 페이지 테이블의 기본 기능을 구현할 것을 권장합니다。

`vm/vm.c`에서 보조 페이지 테이블 관리 함수를 구현하세요。

먼저 보조 페이지 테이블의 설계를 결정해야 합니다. 설계를 마쳤다면 아래 세 가지 함수를 설계에 맞게 구현하세요。

```c
void supplemental_page_table_init (struct supplemental_page_table *spt);
```
보조 페이지 테이블을 초기화합니다. 어떤 자료구조를 사용할지는 자유입니다. 이 함수는 새로운 프로세스가 시작될 때(userprog/process.c의 `initd`)와 프로세스가 포크될 때(userprog/process.c의 `__do_fork`) 호출됩니다。

```c
struct page *spt_find_page (struct supplemental_page_table *spt, void *va);
```
주어진 보조 페이지 테이블에서 가상 주소 `va`에 해당하는 `struct page`를 찾아 반환합니다. 실패 시 `NULL`을 반환하세요。

```c
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
```
주어진 보조 페이지 테이블에 `struct page`를 삽입합니다. 이 함수는 동일한 가상 주소가 이미 보조 페이지 테이블에 존재하지 않는지 확인해야 합니다。

## 프레임 관리

이제부터 페이지는 단순한 메타데이터 이상의 역할을 합니다. 따라서 물리 메모리를 관리하는 별도의 체계가 필요합니다. `include/vm/vm.h`에는 물리 메모리를 나타내는 `struct frame`이 정의되어 있으며, 템플릿에서는 다음과 같습니다：

```c
/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
};
```

현재는 `kva`(커널 가상 주소)와 `page`(연결된 페이지 구조체) 두 필드만 있습니다. 프레임 관리 인터페이스를 구현하면서 필요한 멤버를 추가할 수 있습니다。

다음 함수들을 `vm/vm.c`에 구현하세요: `vm_get_frame`, `vm_claim_page`, `vm_do_claim_page`。

```c
static struct frame *vm_get_frame (void);
```
`palloc_get_page`를 호출하여 사용자 풀에서 새로운 물리 페이지를 얻습니다. 페이지를 성공적으로 얻으면 프레임을 할당하고 멤버들을 초기화한 뒤 반환합니다. `vm_get_frame`를 구현한 이후에는 모든 사용자 공간 페이지 할당을 이 함수를 통해 하도록 해야 합니다. 페이지 할당 실패 시 스왑 아웃을 처리할 필요는 없으며, 당장은 그런 경우를 `PANIC("todo")`로 표시하면 됩니다。

```c
bool vm_do_claim_page (struct page *page);
```
프레임을 확보하고(page용) MMU를 설정하여 가상 주소에서 물리 주소로의 매핑을 추가합니다. 먼저 `vm_get_frame`를 호출해 프레임을 얻은 뒤, 페이지 테이블에 매핑을 추가해야 합니다. 반환값은 성공 여부를 나타냅니다。

```c
bool vm_claim_page (void *va);
```
`va`에 대한 페이지를 할당(claim)합니다. 먼저 해당 `va`에 대한 `struct page`를 찾거나 생성한 다음 `vm_do_claim_page`를 호출하세요。

