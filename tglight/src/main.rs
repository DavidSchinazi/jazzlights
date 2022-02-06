#[macro_use]
extern crate log;
#[macro_use]
extern crate clap;

extern crate ansi_term;
extern crate atty;
extern crate libc;
extern crate time;
extern crate walkdir;
extern crate ws;

mod logger;
mod webui;

use clap::{App, AppSettings, Arg};
use std::thread;

mod player {
    use std::ffi::{CStr, CString};
    mod ffi {
        use std::os::raw::c_char;

        extern "C" {
            pub fn call(cmd: *const c_char) -> *const c_char;
            pub fn run(verbose: u8, ver: *const c_char, cfgfile: *const c_char);
        }
    }
    pub fn call(cmd: &str) -> String {
        let req = CString::new(cmd).expect("corrupted command").into_raw();
        unsafe { CStr::from_ptr(ffi::call(req)) }
            .to_string_lossy()
            .to_string()
    }
    pub fn run(verbose: bool, cfgfile: &str) {
        let cfgfile = CString::new(cfgfile)
            .expect("corrupted config file path")
            .into_raw();
        let ver = CString::new(crate_version!())
            .expect("version can't be turned into C string")
            .into_raw();
        unsafe { ffi::run(if verbose { 1 } else { 0 }, ver, cfgfile) };
    }
}

fn main() {
    const APPNAME: &'static str = "TechnoGecko LED Control";
    const HELPHELP: &'static str = "Print help message and exit";

    let args = App::new(APPNAME)
        .setting(AppSettings::ColoredHelp)
        .setting(AppSettings::UnifiedHelpMessage)
        .setting(AppSettings::DeriveDisplayOrder)
        .version(crate_version!())
        .version_message("Print version and exit")
        .help_message(HELPHELP)
        .arg(
            Arg::with_name("verbose")
                .long("verbose")
                .short("v")
                .global(true)
                .help("Enable verbose log output"),
        )
        .arg(
            Arg::with_name("nocolor")
                .long("nocolor")
                .global(true)
                .help("Disable colors in log output"),
        )
        .arg(
            Arg::with_name("timestamp")
                .long("timestamp")
                .global(true)
                .help("Enable timestamps in log output"),
        )
        .arg(
            Arg::with_name("config")
                .takes_value(true)
                .long("config")
                .global(true)
                .help("Use given configuration file"),
        )
        .arg(
            Arg::with_name("listen")
                .takes_value(true)
                .long("listen")
                .help("Bing HTTP server to given host and port, defaults to 0.0.0.0:8080"),
        )
        .get_matches();

    logger::init(
        args.occurrences_of("verbose") > 0,
        args.occurrences_of("nocolor") > 0,
        args.occurrences_of("timestamp") > 0,
    );

    let http_listen = args
        .value_of("listen")
        .unwrap_or("0.0.0.0:8080")
        .to_string();

    info!("{} v{}", APPNAME, crate_version!());

    thread::spawn(move || {
        webui::run(http_listen);
    });
    player::run(
        args.occurrences_of("verbose") > 0,
        args.value_of("config").unwrap_or("tglight.toml"),
    );
}
