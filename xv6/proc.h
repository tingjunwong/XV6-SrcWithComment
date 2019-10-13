// Segments in proc->gdt.
// ȫ�������������ܹ���7��
#define NSEGS     7

// Per-CPU state
/*
 * ����ṹ�����Ŀ���Ǵ洢CPU״̬
 */
struct cpu {
  uchar id;                    // Local APIC ID; index into cpus[] below
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  
  // Cpu-local storage variables; see below
  struct cpu *cpu;
  struct proc *proc;           // The currently-running process.
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Per-CPU variables, holding pointers to the
// current cpu and to the current process.
// The asm suffix tells gcc to use "%gs:0" to refer to cpu
// and "%gs:4" to refer to proc.  seginit sets up the
// %gs segment register so that %gs refers to the memory
// holding those two variables in the local cpu's struct cpu.
// This is similar to how thread-local variables are implemented
// in thread libraries such as Linux pthreads.
/*
 * ��һ��û����ɶ��˼������
 */
extern struct cpu *cpu asm("%gs:0");       // &cpus[cpunum()]
extern struct proc *proc asm("%gs:4");     // cpus[cpunum()].proc

//PAGEBREAK: 17�����ɶ��˼����
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// ��Contexts are stored at the bottom of the stack they
// describe������仰ɶ��˼����; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates���ٿأ� it.
/*
 * ������ʵ�������ں˽����������л���Ҫ�õ��ļ����Ĵ���
 */
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

/*
 * ö�����ͣ�ö���˽��̵�6��״̬��UNUSEDδʹ��̬��EMBRYO��ʼ̬��SLEEPING�ȴ�̬��RUNNABLE����̬��RUNNING����̬��ZOMBIE��ʬ̬��
 * ״̬�ĺ������£�
 * UNUSED������δ����������PCB���У�ʱ��״̬���Ҹо�������ʼ��״̬������������ҵĸо��ǣ�PCB��һ��ʼ�ʹ��ڵģ�ֻ������ϵͳ���󴴽�����ʱ�ŻὫ���е�PCB�����������̣�
 * EMBRYO����Ҫ����һ�����̿��ƿ����ҵ�һ������UNUSED״̬�Ľ��̿��ƿ�ʱ���Ѵ˽��̿��ƿ�״̬����ΪҪʹ�õ�״̬��
 * SLEEPING���������ڵȴ�ĳ��Դ��ԭ���޷�ִ�У�����˯��״̬�����ȴ�̬����ӡ�������ʱ��ͻ�ѳ���д����̣���֪���ǲ��Ǻ��м�����Ū���ˣ��м����ȵ�ȷ�ǽ���ʱ�����еĽ��̴��ڴ�д����棩��
 * RUNNABLE�����̻���˳�CPU֮���������Դ�����ڿ�����״̬��������̬�����ʱ�����һ���������У����ھ���̬�Ľ��̰������CPU������ռ��ǰ���£���
 * RUNNING�����̻��CPU���������е�״̬����ִ��̬��
 * ZOMBIE�����̽�����״̬��
 * ״̬ת��ͼ��https://github.com/wsxst/XV6-SrcWithComment/blob/master/XV6%E8%BF%9B%E7%A8%8B%E7%8A%B6%E6%80%81%E8%BD%AC%E6%8D%A2%E5%9B%BE.vsdx����Ҫ��visio�򿪡�
 *
 * ���Linux�н���״̬�е�����
 * Linux�ں��ж��������¼���״̬��
 * #define TASK_RUNNING 0
 * #define TASK_INTERRUPTIBLE 1
 * #define TASK_UNINTERRUPTIBLE 2
 * #define TASK_ZOMBIE 4
 * #define TASK_STOPPED 8
 * ���У�
 * TASK_RUNNING�Ǿ���̬�����̵�ǰֻ�ȴ�CPU��Դ��������Դ�Ѿ�ȫ����λ��
 * TASK_INTERRUPTIBLE��TASK_UNINTERRUPTIBLE��������̬�����̵�ǰ���ڵȴ���CPU�������ϵͳ��Դ�����ߵ���������ǰ�߿��Ա��źŻ��ѣ����߲����ԡ�
 * TASK_ZOMBIE�ǽ�ʬ̬�������Ѿ��������У����ǽ��̿��ƿ���δע�������˸о�����һ���м��һ����ʱ״̬��
 * TASK_STOPPED�ǹ���״̬����Ҫ���ڵ���Ŀ�ģ���������ν������ǰѵ�ǰ���������̣����̽��յ�SIGSTOP�źź������״̬���ڽ��յ�SIGCONT���ֻ�ָ����С�
 */
enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
/* 
 * ʹ������ṹ����ά��һ�����̵�״̬��
 * ������Ϊ��Ҫ��״̬�ǽ��̵�ҳ���ں�ջ����ǰ����״̬��
 */
struct proc {
  uint sz;                     //��BΪ��λ����¼������ռ�е��ڴ�ռ�Ĵ�С
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process �������ں�̬��ջ
  enum procstate state;        // Process state
  /*
   * volatile�ı����ǡ��ױ�ġ� ��Ϊ���ʼĴ���Ҫ�ȷ����ڴ浥Ԫ��Ķ�,���Ա�����һ�㶼�������ٴ�ȡ�ڴ���Ż������п��ܻ�������ݡ���Ҫ��ʹ��volatile��������ֵ��ʱ��ϵͳ�������´������ڵ��ڴ��ȡ���ݣ���ʹ��ǰ���ָ��ոմӸô���ȡ�����ݡ�
   * ��ȷ��˵���ǣ���������ؼ��������ı������������Է��ʸñ����Ĵ���Ͳ��ٽ����Ż����Ӷ������ṩ�������ַ���ȶ����ʣ������ʹ��valatile����������������������������Ż���
   */
  volatile int pid;            // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall �жϽ��̺���Ҫ�ָ����̼���ִ��������ļĴ�������
  struct context *context;     // swtch() here to run process �л�����ʱ����Ҫά����Ӳ���Ĵ�������
  void *chan;                  // If non-zero, sleeping on chan ����ñ���ֵ��ΪNULL�����ʾ����˯��̬ʱ�����ڵ�˯�߶��У���ͷָ�룩
  int killed;                  // If non-zero, have been killed ����ñ���ֵ��Ϊ0�����ʾ�ý����Ѿ���ɱ����
  struct file *ofile[NOFILE];  // Open files ���̴򿪵��ļ�����
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging) ���ֵҲ�͵���ʱ���ã��Ҿ����������ÿ����߷��㿴���������ĸ����̵���Ϊ
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
