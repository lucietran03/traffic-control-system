Act as a Senior Embedded Systems Engineer specializing in BlackBerry QNX Neutrino RTOS (7.x/8.x) and safety-critical system design.

I have defined the scope and requirements for our QNX project below. Based on this, provide a concise, production-ready technical architecture and implementation plan answering:

1. System Architecture ("What"):
   - Process breakdown (Resource Managers, device drivers, application services).
   - Threading model, POSIX priority assignments, and scheduling policies (FIFO/Round-Robin).
2. QNX Implementation Details ("How"):
   - Native QNX IPC strategy (Channels, Connections, MsgSend/Receive/Reply vs. Pulses vs. Shared Memory).
   - Hardware abstraction and POSIX / devctl interface design.
   - Fault handling, watchdog monitoring, and fail-safe state transitions.
3. Module Implementation Sequence:
   - Ordered checklist of components to build, from core IPC/resource managers to business logic.

Do not generate generic Linux code. Adhere strictly to native QNX microkernel conventions and best practices.

my project scope and requirements and architec in in the md file, reading all of the m
