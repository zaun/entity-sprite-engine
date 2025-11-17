# HTTP Lua API

The `HTTP` API provides Lua access to simple asynchronous HTTP GET requests.

---

## Global `HTTP` Table

- `HTTP.new(url_string)` → creates a new request for the given `http://` URL
- `HTTP.STATUS` → constants table with common statuses: `OKAY=200`, `BAD_REQUEST=400`, `NOT_FOUND=404`, `INTERNAL_SERVER_ERROR=500`, `UNKNOWN=-1`, `IN_PROGRESS=0`

---

## Request Object Properties

Read-only properties:
- `url` → string
- `status` → integer (`HTTP.STATUS.*` or actual status; -1 on error, 0 while pending)
- `body` → string (response body; empty until done)
- `headers` → string (raw response headers; empty until done)
- `done` → boolean (true when completed or failed)

Writable property via method:
- `set_timeout(ms)` → sets connection/receive timeouts in milliseconds (0 = system default)

Methods:
- `start()` → begins the request asynchronously; returns nothing; request frees itself after completion

---

## Example

```lua
local req = HTTP.new("http://example.com")
req:set_timeout(5000)
req:start()

-- Polling example (engine-specific frame loop)
while not req.done do
  -- wait
end

print("status:", req.status)
print("headers:\n" .. req.headers)
print("body:\n" .. req.body)
```


