I want to make a documentation-only cleanup to the code based on the latest commit.

IMPORTANT:
This is NOT a functional refactor.
Do NOT change any code behavior, logic, control flow, API calls, variables, conditions, function signatures, or formatting unless it is strictly required to add comments.

First, inspect the latest commit on the current branch and identify the code changes introduced by that commit.

The latest commit mainly added step-by-step explanatory comments to the implementation. I want to apply the SAME commenting style consistently to the relevant code.

Example of the intended style:

BEFORE:

for (;;) {

    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && !q->stopping) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    if (q->count == 0 && q->stopping) {
        pthread_mutex_unlock(&q->lock);
        break;
    }

    job = q->jobs[q->head];
    q->head = (q->head + 1) % IPC_CLIENT_QUEUE_CAPACITY;
    q->count--;

    pthread_mutex_unlock(&q->lock);

    // Attempt to open the target node and send the request.
    send_ok = 0;

    if (build_open_path(job.target_id, path, sizeof(path)) == 0) {
        coid = name_open(path, 0);

        if (coid == -1 && strstr(path, "/dev/name/global/") != NULL) {
            char alt_path[IPC_OPEN_PATH_MAX];
            char *g = strstr(path, "/dev/name/global/");

            snprintf(...);

            coid = name_open(alt_path, 0);
        }

        if (coid != -1) {
            send_ok =
                (MsgSend(coid, &job.req, sizeof(job.req),
                         &reply, sizeof(reply)) != -1);

            name_close(coid);
        }
    }
```

AFTER:
```
// Loop indefinitely, processing jobs from the queue.
for (;;) {

    // #1 Wait for a job to be available in the queue.
    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && !q->stopping) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    // #2 If the queue is stopping and empty, exit the thread.
    if (q->count == 0 && q->stopping) {
        pthread_mutex_unlock(&q->lock);
        break;
    }

    // #3 Dequeue the job from the head of the buffer.
    job = q->jobs[q->head];
    q->head = (q->head + 1) % IPC_CLIENT_QUEUE_CAPACITY;
    q->count--;

    pthread_mutex_unlock(&q->lock);

    // #4 Attempt to open the target node and send the request.
    send_ok = 0;

    if (build_open_path(job.target_id, path, sizeof(path)) == 0) {
        coid = name_open(path, 0); // Attempt to open the target node

        if (coid == -1 && strstr(path, "/dev/name/global/") != NULL) {
            char alt_path[IPC_OPEN_PATH_MAX]; // Fallback to local namespace if global open fails
            char *g = strstr(path, "/dev/name/global/"); // Find the global path segment

            snprintf(...);

            coid = name_open(alt_path, 0); // Attempt to open the local fallback path
        }

        if (coid != -1) {
            // Send the request and wait for a reply, capturing the success status.
            send_ok =
                (MsgSend(coid, &job.req, sizeof(job.req),
                         &reply, sizeof(reply)) != -1);

            name_close(coid);
        }
    }
```

TASK:

1. Inspect the latest commit and identify the files/functions where this commenting style was introduced.
2. Apply the same style to the relevant surrounding implementation where useful.
3. Focus on explaining the major execution steps and non-obvious QNX/POSIX operations.
4. Use numbered comments such as:
   - `// #1 ...`
   - `// #2 ...`
   - `// #3 ...`
   when a function has a clear sequential workflow.
5. Use short inline comments for non-obvious operations such as:
   - `name_open()`
   - `name_close()`
   - `MsgSend()`
   - `MsgReceive()`
   - `MsgReply()`
   - Qnet path construction
   - fallback namespace handling
   - mutex/condition-variable operations
   - queue operations
   - timers/pulses
   - heartbeat/watchdog handling

COMMENTING RULES:

- Comments should explain WHAT the code is doing and, where useful, WHY.
- Keep comments short and readable.
- Do not explain obvious C syntax.
- Do not add comments to every single line.
- Prefer comments at meaningful execution steps.
- Use the existing code terminology.
- Do not introduce terminology that does not already exist in the implementation.
- Do not duplicate comments that are already clear.
- Keep the style consistent with the example above.

MOST IMPORTANT:

Do NOT modify the implementation itself.

That means:
- no logic changes
- no variable changes
- no function changes
- no API changes
- no condition changes
- no error-handling changes
- no formatting refactor unrelated to comments
- no renaming
- no architecture changes

Only add or improve comments.

Before making changes, compare the current code with the latest commit so you understand exactly what was added.

After editing, provide:

1. A concise summary of which files/functions received comments.
2. A diff showing that the changes are comment-only.
3. Confirmation that no executable code or behavior was changed.