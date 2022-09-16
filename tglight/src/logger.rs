use ansi_term::Colour::{Red,Cyan,Yellow};
use ansi_term;
use atty;
use log::{self,Log,Level,Metadata,Record,LevelFilter};
use time;
//use player;

struct Logger {
    level: Level,
    timestamps: bool,
    nocolor: bool,
}

impl Log for Logger {
    fn enabled(&self, metadata: &Metadata) -> bool {
        metadata.level() <= self.level
    }

    fn log(&self, record: &Record) {
       if self.enabled(record.metadata()) {
            let level = format!("{:<5}", record.level());
            let msg = format!("{}", record.args());
            let ts = if self.timestamps {
                time::strftime("%Y-%m-%d %H:%M:%S ", &time::now()).unwrap()
            } else {
                String::from("")
            };

            if self.nocolor {
                eprintln!("{}{:<5} {}", ts, level, msg);
            }
            else {
                let style = match record.level() {
                    Level::Debug => Cyan.into(),
                    Level::Warn => Yellow.into(),
                    Level::Error => Red.into(),
                    _ => ansi_term::Style::default(),
                };
                let msg = style.paint(msg);
                let level = style.bold().paint(level);
                eprintln!("{}{:<5} {}", ts, level, msg);
            }
       }
    }

    fn flush(&self) {
    }
}

pub fn init(verbose : bool, nocolor: bool, timestamps: bool) {
    let level = if verbose { 
        Level::Debug
    } else {
        Level::Info
    };

    // if verbose {
    //     player::set_verbose();
    // }

    // player::set_log_handler(player_msg_handler);

    #[cfg(windows)]
    let nocolor = nocolor || !enable_ansi_support(); 
    let nocolor = nocolor || !atty::is(atty::Stream::Stderr);

    log::set_boxed_logger(Box::new(Logger{level, nocolor, timestamps})).unwrap();
    log::set_max_level(LevelFilter::Trace);
    debug!("Log level set to {}, colors:{}, timestamps:{}", level, !nocolor, timestamps);
}
