# Concurrent Transactional Key-Value Store

> A crash-proof, multi-client key-value store that keeps every transaction honest — even when dozens of clients are fighting over the same keys.

---

##  What We Built

A key-value store that runs as a **server**, accepts **many clients at once over TCP**, and keeps all of their transactions properly isolated. It's deliberately *not* a SQL database — no tables, no joins, no queries, just a flat store of keys and values. The main focus was on making **concurrency** and **durability** both work simultaneosly and bug free.

Clients connect, speak a simple text protocol, and run transactions:

```
BEGIN  →  read / write keys  →  COMMIT or ABORT
```

Many clients can hold open transactions **simultaneously**, and the server makes sure they never trip over each other.

---

##  The Two Properties That Matter

| Property | What it guarantees |
|---|---|
| **Durability** | Once the server says *"committed,"* that data survives any later crash — even one microseconds afterward. |
| **Serializability** | No matter how transactions interleave, the result always matches *some* valid order of running them one by one. |

> Either one alone is easy. Getting **both at once, with real performance**, Which is what a real Database should cater to. 

---

##  What It Does

-  **Real isolation** — two clients editing the same keys get consistent results and never see each other's uncommitted changes.
-  **Two isolation levels** — clients can trade strictness for speed depending on the workload.
-  **Deadlock handling** — when transactions get stuck waiting on each other, the server detects it and aborts one so the rest move on.
-  **Batched commits** — many commits share the cost of writing to disk, making the server *dramatically* faster under load than committing one at a time.
-  **Automatic crash recovery** — on restart, the server restores exactly the transactions that were acknowledged and ignores every one that wasn't.

---

##  Proving It Works

| Tool | What it does |
|---|---|
| **Chaos test** | Throws adversarial concurrent workloads at a live server, then checks the result against an independent calculation after every run. |
| **Benchmark** | Measures throughput and latency across single-client, high-concurrency, and read-heavy conditions. |
