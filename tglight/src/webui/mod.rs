// use std::process::Command;
use player;
use std::ptr;
use ws::{listen, util::Token, Handler, Handshake, Message, Request, Response, Result, Sender};

const GRATI: Token = Token(1);
static INDEX_HTML: &'static [u8] = include_bytes!("index.html");

// Server web application handler
struct Server {
    out: Sender,
}

impl Handler for Server {
    //
    fn on_request(&mut self, req: &Request) -> Result<Response> {
        // Using multiple handlers is better (see router example)
        match req.resource() {
            // The default trait implementation
            "/ws" => Response::from_request(req),

            // Create a custom response
            "/" => Ok(Response::new(200, "OK", INDEX_HTML.to_vec())),

            _ => Ok(Response::new(404, "Not Found", b"404 - Not Found".to_vec())),
        }
    }

    // Handle messages recieved in the websocket (in this case, only on /ws)
    fn on_message(&mut self, msg: Message) -> Result<()> {
        self.out.broadcast(player::call(msg.as_text().unwrap())).unwrap();
        Ok(())
    }

    fn on_open(&mut self, _: Handshake) -> Result<()> {
        info!("Opened connection to {:?}", self.out.token());
        self.out.timeout(200, GRATI).unwrap();
        self.out.broadcast(player::call("sysinfo?")).unwrap();
        self.out.broadcast(player::call("status?"))
    }

    fn on_timeout(&mut self, _event: Token) -> Result<()> {
        self.out.timeout(200, GRATI).unwrap();
        self.out.broadcast(player::call("sysinfo?")).unwrap();
        self.out.broadcast(player::call("status?"))
    }

    fn on_shutdown(&mut self) {
        info!("Closing connection to {:?}", self.out.token());
    }
}

static mut SERVER: *const Server = ptr::null();

pub fn run(bind_addr: String) {
    info!("Starting HTTP server on {}...", bind_addr);
    listen(bind_addr, |out| {
        let s = Server { out };
        unsafe {
            SERVER = &s;
        }
        return s;
    })
    .unwrap()
}
