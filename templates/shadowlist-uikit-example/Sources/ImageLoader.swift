import UIKit

/*
 * Loads remote images off the main thread, decodes them at the size they show at, and keeps
 * the decoded images in memory. A view that gets a new URL before the old one arrives drops
 * the old result.
 */
final class ImageLoader {
  static let shared = ImageLoader()

  private let cache = NSCache<NSString, UIImage>()
  private let session: URLSession
  private let queue = DispatchQueue(label: "image.decode", qos: .userInitiated, attributes: .concurrent)
  private var waiting: [String: [(UIImage?) -> Void]] = [:]

  private init() {
    let configuration = URLSessionConfiguration.default
    configuration.urlCache = URLCache(memoryCapacity: 32 << 20, diskCapacity: 512 << 20)
    configuration.requestCachePolicy = .returnCacheDataElseLoad
    session = URLSession(configuration: configuration)
    cache.totalCostLimit = 96 << 20
  }

  func cached(_ url: URL, size: CGSize) -> UIImage? {
    cache.object(forKey: Self.key(url, size) as NSString)
  }

  private static func key(_ url: URL, _ size: CGSize) -> String {
    "\(url.absoluteString)#\(Int(size.width))x\(Int(size.height))"
  }

  /*
   * Calls done on the main thread with the decoded image, or nil on failure.
   */
  func load(_ url: URL, size: CGSize, done: @escaping (UIImage?) -> Void) {
    let key = Self.key(url, size)
    if let image = cache.object(forKey: key as NSString) {
      done(image)
      return
    }
    if waiting[key] != nil {
      waiting[key]!.append(done)
      return
    }
    waiting[key] = [done]
    let scale = UIScreen.main.scale
    session.dataTask(with: url) { [weak self] data, _, _ in
      guard let self else { return }
      self.queue.async {
        var image: UIImage?
        if let data, let source = UIImage(data: data) {
          let pixels = CGSize(width: size.width * scale, height: size.height * scale)
          image = source.preparingThumbnail(of: Self.fill(source.size, into: pixels)) ?? source.preparingForDisplay()
        }
        DispatchQueue.main.async {
          if let image {
            self.cache.setObject(image, forKey: key as NSString, cost: Int(image.size.width * image.size.height * image.scale * image.scale * 4))
          }
          let callbacks = self.waiting.removeValue(forKey: key) ?? []
          callbacks.forEach { $0(image) }
        }
      }
    }.resume()
  }

  /*
   * The smallest size with the source's aspect that covers the target.
   */
  private static func fill(_ source: CGSize, into target: CGSize) -> CGSize {
    guard source.width > 0, source.height > 0 else { return target }
    let scale = max(target.width / source.width, target.height / source.height)
    return CGSize(width: ceil(source.width * scale), height: ceil(source.height * scale))
  }
}

final class RemoteImageView: UIImageView {
  private var url: URL?

  override init(frame: CGRect) {
    super.init(frame: frame)
    contentMode = .scaleAspectFill
    clipsToBounds = true
  }

  convenience init() {
    self.init(frame: .zero)
  }

  required init?(coder: NSCoder) { fatalError() }

  func setImage(_ url: URL?, size: CGSize) {
    guard url != self.url || image == nil else { return }
    self.url = url
    image = nil
    guard let url, Settings.images else { return }
    if let cached = ImageLoader.shared.cached(url, size: size) {
      image = cached
      return
    }
    ImageLoader.shared.load(url, size: size) { [weak self] image in
      guard let self, self.url == url else { return }
      self.image = image
    }
  }
}
