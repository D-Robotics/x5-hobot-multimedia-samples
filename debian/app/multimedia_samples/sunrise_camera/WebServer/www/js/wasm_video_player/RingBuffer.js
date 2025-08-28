class NALUPointerRingBuffer {
    constructor(capacity) {
        this.buffer = new Array(capacity); // 预分配固定大小的队列
        this.capacity = capacity;
        this.head = 0; // 读指针
        this.tail = 0; // 写指针
        this.size = 0; // 当前存储的 NALU 数量
    }

    // **存储 NALU 的引用**
    push(nalu) {
        if (this.size === this.capacity) {
            console.warn("Buffer full, discarding all oldest NALU");
            // this.pop(); // 丢弃最旧的 NALU
			this.clear(); // 直接清空整个缓冲区
        }

        this.buffer[this.tail] = nalu; // 仅存储 NALU 的引用
        this.tail = (this.tail + 1) % this.capacity;
        this.size++;
    }
	clear() {
        this.buffer.fill(null); // 显式释放引用（避免内存泄漏）
        this.head = 0;
        this.tail = 0;
        this.size = 0;
    }

    // **取出 NALU 的引用**
    pop() {
        if (this.size === 0) return null;

        const nalu = this.buffer[this.head]; // 直接返回引用
        this.buffer[this.head] = null; // 清空存储（防止内存泄漏）
        this.head = (this.head + 1) % this.capacity;
        this.size--;

        return nalu;
    }

    // **获取当前缓冲区状态**
    getBufferStatus() {
        return {
            capacity: this.capacity,
            used: this.size,
            free: this.capacity - this.size,
            head: this.head,
            tail: this.tail
        };
    }
}