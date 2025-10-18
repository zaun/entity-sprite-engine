function ENTITY:entity_init()
    -- Test both HTTP and HTTPS
    self.data.http_req = HTTP.new("http://httpbin.org/get")
    self.data.https_req = HTTP.new("https://httpbin.org/get")
    
    if self.data.http_req then
        print("HTTP request created successfully")
        self.data.http_req.start()
        print("HTTP request started")
    else
        print("Failed to create HTTP request")
    end
    
    if self.data.https_req then
        print("HTTPS request created successfully")
        self.data.https_req.start()
        print("HTTPS request started")
    else
        print("Failed to create HTTPS request")
    end
end

function ENTITY:entity_update(delta_time)
    -- Check HTTP request
    if self.data.http_req and self.data.http_req.done == true then
        print("=== HTTP Request Complete ===")
        print("Final URL: " .. self.data.http_req.url)
        print("Status: " .. self.data.http_req.status)
        print("Body: " .. (self.data.http_req.body or "empty"))
        print("Headers: " .. (self.data.http_req.headers or "empty"))
        print("=============================")
        self.data.http_req = nil
    end
    
    -- Check HTTPS request
    if self.data.https_req and self.data.https_req.done == true then
        print("=== HTTPS Request Complete ===")
        print("Final URL: " .. self.data.https_req.url)
        print("Status: " .. self.data.https_req.status)
        print("Body: " .. (self.data.https_req.body or "empty"))
        print("Headers: " .. (self.data.https_req.headers or "empty"))
        print("==============================")
        self.data.https_req = nil
    end
end
